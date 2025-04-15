/**
 * initialize.c
 *
 * Contains functions that initialize various hardware and software components.
 */

#include "initialize.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart_vfs.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_vfs_dev.h"
#include "esp_wifi_default.h"
#include "esp_wifi.h"
#include "freertos/freeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "animations.h"
#include "led_matrix.h"
#include "pinout.h"
#include "main_types.h"
#include "app_errors.h"
#include "app_nvs.h"
#include "ota.h"
#include "utilities.h"
#include "strobe_task.h"
#include "strobe.h"
#include "strobe_types.h"
#include "action_task.h"
#include "routines.h"
#include "refresh.h"
#include "wifi.h"

#define TAG "init"

#define USB_SERIAL_BUF_SIZE (1024)

static void initializeMainState(MainTaskState *state);

#if CONFIG_HARDWARE_VERSION == 1
/* no version specific functions */
#elif CONFIG_HARDWARE_VERSION == 2
static esp_err_t beginLoadingAnimation(void);
static esp_err_t endLoadingAnimation(void);
#else
#error "Unsupported hardware version!"
#endif

/**
 * @brief Initializes global static resources, software components, and fields
 *        of state and res.
 *
 * @note Queries user for settings if none are found in non-volatile storage.
 *
 * @requires:
 *  - initializeIndicatorLEDs is executed.
 *
 * @param[in] state A pointer to the main task state.
 * @param[in] res A pointer to resources for the main task.
 *
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid arguments.
 *          ESP_ERR_NO_MEM if an allocation error occurred.
 *          ESP_FAIL if an unexpected error occurred.
 */
esp_err_t initializeApplication(MainTaskState *state, MainTaskResources *res)
{
    /* global resources */
    static UserSettings settings;

    /* local variables */
    esp_err_t err = ESP_FAIL;
    esp_tls_t *tls = NULL;
    TaskHandle_t otaTask = NULL;
    wifi_init_config_t default_wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    nvs_handle_t workerHandle;

    /* input guards */
    if (state == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (res == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);

    /* initialize state and resources to known values */
    initializeMainState(state);
    res->nvsHandle = (nvs_handle_t) NULL;
    res->refreshTimer = NULL;

    /* initialize components and resources */
    err = initLedMatrix();
    if (err != ESP_OK) return err;
    err = initAppErrors();
    if (err != ESP_OK) return err;
    err = calculateLEDSequences();
    if (err != ESP_OK) return err;

    settings.wifiSSID = NULL;
    settings.wifiSSIDLen = 0;
    settings.wifiPass = NULL;
    settings.wifiPassLen = 0;
    res->settings = &settings;

    /* initialize indicator LEDs */
    err = initializeIndicatorLEDs();
    if (err != ESP_OK) return err;

    /* begin loading animation */
#ifdef CONFIG_SUPPORT_STROBING
    err = createStrobeTask(NULL); // need this task to start early
    if (err != ESP_OK) return err;
#endif

#if CONFIG_HARDWARE_VERSION == 1
    /* loading animation unsupported */
#elif CONFIG_HARDWARE_VERSION == 2
    /* begin loading animation */
    err = beginLoadingAnimation();
    if (err != ESP_OK) return err;
#else
#error "Unsupported hardware version!"
#endif

    /* initialize and cleanup non-volatile storage */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_LOGE(TAG, "no free pages in nvs, need to erase nvs partition with parttool.py.");
    }
    if (err != ESP_OK) THROW_ERR(err);

    res->nvsHandle = openMainNvs();
    if (res->nvsHandle == (nvs_handle_t) NULL) THROW_ERR(ESP_FAIL);
    err = removeExtraMainNvsEntries(res->nvsHandle); // keep handle open
    if (err != ESP_OK) return err;

    workerHandle = openWorkerNvs();
    if (workerHandle == (nvs_handle_t) NULL) THROW_ERR(ESP_FAIL);
    err = removeExtraWorkerNvsEntries(workerHandle); // keep handle open
    if (err != ESP_OK) return err;

    /* check if a settings update is requested or necessary */
    err = gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT); // pin has external pullup
    if (err != ESP_OK) THROW_ERR(ESP_FAIL);
    err = retrieveNvsEntries(res->nvsHandle, &settings);
    if (gpio_get_level(T_SW_PIN) == 0 || err != ESP_OK)
    {
        ESP_LOGI(TAG, "updating settings, err: %d", err);
        updateNvsSettings(res->nvsHandle);
    }

    /* retrieve nvs settings */
    err = nvsEntriesExist(res->nvsHandle);
    if (err != ESP_OK) return err;
    err = retrieveNvsEntries(res->nvsHandle, &settings);
    if (err != ESP_OK) return err;

    /* initialize tcp/ip stack */
    err = esp_netif_init();
    if (err != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK) return err;
    (void) esp_netif_create_default_wifi_sta(); // don't need result

    /* establish wifi connection & tls */
    err = esp_wifi_init(&default_wifi_cfg);
    if (err != ESP_OK) return err;
    err = initWifi(res->settings->wifiSSID, res->settings->wifiPass);
    if (err != ESP_OK) return err;
    err = establishWifiConnection();
    while (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
        /* nvs does not have enough space for wifi. I conclude this is due
        to fragmentation, so simply erasing everything is a good enough fix.
        This should really only occur when the user has just changed wifi
        settings, so I don't think there is a risk of killing non-wifi 
        operation here by deleting stored data */
        ESP_LOGE(TAG, "erasing nvs");
        err = nvs_erase_all(res->nvsHandle); // keep handle open
        if (err != ESP_OK) return err;
        err = nvs_erase_all(workerHandle); // close handle
        if (err != ESP_OK) return err;

        ESP_LOGE(TAG, "rewriting user settings to nvs");
        err = storeNvsSettings(res->nvsHandle, *res->settings);
        if (err != ESP_OK) return err;

        esp_restart();
    } // ignore other error codes

    tls = esp_tls_init();
    if (tls == NULL)
        return ESP_ERR_NO_MEM;

    /* initialize data */
    err = initRefresh();
    if (err != ESP_OK) return err;

    /* create tasks */
    err = createActionTask(NULL);
    if (err != ESP_OK) return err;
    err = createOTATask(&otaTask);
    if (err != ESP_OK) return err;

    /* create refresh timer */
    state->toggle = false;
    res->refreshTimer = createRefreshTimer(xTaskGetCurrentTaskHandle(), &(state->toggle));
    if (res->refreshTimer == NULL) return ESP_FAIL;

    /* initialize buttons */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK) THROW_ERR(err);
    err = initIOButton(otaTask);
    if (err != ESP_OK) return err;
    err = initDirectionButton(&(state->toggle));
    if (err != ESP_OK) return err;

#if CONFIG_HARDWARE_VERSION == 1
    /* loading animation unsupported */
#elif CONFIG_HARDWARE_VERSION == 2
    /* end loading animation */
    err = endLoadingAnimation();
    if (err != ESP_OK) return err;
#else
#error "Unsupported hardware version!"
#endif

    return ESP_OK;
}

#if CONFIG_HARDWARE_VERSION == 1

/**
 * @brief Initializes communication through the USB connector.
 *
 * @note For V1_0, communication is achieved through use of UART 0.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initializeLogChannel(void)
{
    esp_err_t err;
    err = uart_driver_install(UART_NUM_0,
                              UART_HW_FIFO_LEN(UART_NUM_0) + 16,
                              UART_HW_FIFO_LEN(UART_NUM_0) + 16,
                              32,
                              NULL,
                              0);
    if (err != ESP_OK)
        return err;
    uart_vfs_dev_use_driver(UART_NUM_0); // enable interrupt driven IO
    return ESP_OK;
}

/**
 * @brief Initializes indicator LEDs to off state.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initializeIndicatorLEDs(void)
{
    esp_err_t err;

    err = gpio_set_direction(WIFI_LED_PIN, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
        return err;
    err = gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
        return err;
    err = gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
        return err;
    err = gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
        return err;
    err = gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
        return err;
    err = gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
        return err;

    err = gpio_set_level(WIFI_LED_PIN, 0);
    if (err != ESP_OK)
        return err;
    err = gpio_set_level(ERR_LED_PIN, 0);
    if (err != ESP_OK)
        return err;
    err = gpio_set_level(LED_NORTH_PIN, 0);
    if (err != ESP_OK)
        return err;
    err = gpio_set_level(LED_EAST_PIN, 0);
    if (err != ESP_OK)
        return err;
    err = gpio_set_level(LED_SOUTH_PIN, 0);
    if (err != ESP_OK)
        return err;
    err = gpio_set_level(LED_WEST_PIN, 0);
    return err;
}

#elif CONFIG_HARDWARE_VERSION == 2

/**
 * @brief Initializes communication through the USB connector. Required for
 *        logging with ESP_LOG family functions.
 *
 * @note For V2_0, communication is achieved through use of the USB peripheral.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initializeLogChannel(void)
{
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .rx_buffer_size = USB_SERIAL_BUF_SIZE,
        .tx_buffer_size = USB_SERIAL_BUF_SIZE,
    };
    vTaskDelay(100); // give usb channel time to connect. I can't find a better way to do this
    return usb_serial_jtag_driver_install(&usb_serial_jtag_config);
}


/**
 * @brief Initializes indicator LEDs to off state.
 *
 * @requires:
 * - led_matrix component initialized.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initializeIndicatorLEDs(void)
{
    esp_err_t err;
    err = matSetScaling(OTA_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(OTA_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK) return err;
    err = matSetScaling(WIFI_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(WIFI_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK) return err;
    err = matSetScaling(ERROR_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(ERROR_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK) return err;
    err = matSetScaling(NORTH_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(NORTH_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK) return err;
    err = matSetScaling(EAST_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(EAST_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK) return err;
    err = matSetScaling(WEST_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(WEST_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK) return err;
    err = matSetScaling(SOUTH_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(SOUTH_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK) return err;
    err = initLEDLegendLight(FAST_RED, FAST_GREEN, FAST_BLUE);
    if (err != ESP_OK) return err;
    err = initLEDLegendMedium(MEDIUM_RED, MEDIUM_GREEN, MEDIUM_BLUE);
    if (err != ESP_OK) return err;
    err = initLEDLegendHeavy(SLOW_RED, SLOW_GREEN, SLOW_BLUE);
    if (err != ESP_OK) return err;
    return err;
}

/**
 * @brief Initializes the Heavy LED in the legend to the provided color.
 *
 * @requires:
 * - led_matrix component initialized.
 *
 * @param[in] red The red component of the color to set the LED.
 * @param[in] green The green component of the color to set the LED.
 * @param[in] blue The blue component of the color to set the LED.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initLEDLegendHeavy(uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err;
    err = matSetScaling(HEAVY_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(HEAVY_LED_NUM, red, green, blue);
    return err;
}

/**
 * @brief Initializes the Medium LED in the legend to the provided color.
 *
 * @requires:
 * - led_matrix component initialized.
 *
 * @param[in] red The red component of the color to set the LED.
 * @param[in] green The green component of the color to set the LED.
 * @param[in] blue The blue component of the color to set the LED.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initLEDLegendMedium(uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err;
    err = matSetScaling(MEDIUM_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(MEDIUM_LED_NUM, red, green, blue);
    return err;
}

/**
 * @brief Initializes the Light LED in the legend to the provided color.
 *
 * @requires:
 * - led_matrix component initialized.
 *
 * @param[in] red The red component of the color to set the LED.
 * @param[in] green The green component of the color to set the LED.
 * @param[in] blue The blue component of the color to set the LED.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initLEDLegendLight(uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err;
    err = matSetScaling(LIGHT_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(LIGHT_LED_NUM, red, green, blue);
    return err;
}

#else
#error "Unsupported hardware version!"
#endif

/**
 * @brief Initializes state to a known state.
 *
 * @param[out] state A pointer to the state to initialize.
 */
static void initializeMainState(MainTaskState *state)
{
    state->toggle = false;
    state->first = true;
#ifdef CONFIG_FIRST_DIR_NORTH
    state->dir = NORTH;
#else
    state->dir = SOUTH;
#endif
}

#if CONFIG_HARDWARE_VERSION == 1

/* no version specific functions */

#elif CONFIG_HARDWARE_VERSION == 2

/**
 * @brief Begins the loading animation by registering strobing on the direction
 * LEDs, with staggered starting values.
 * 
 * @requires:
 * - strobe task created.
 * - led_matrix component initialized.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if any requirements are unmet.
 * ESP_ERR_TIMEOUT if an I2C transaction failed.
 * ESP_ERR_INVALID_RESPONSE if a matrix page could not be set.
 * APP_ERR_MUTEX_RELEASE if a mutex failed to release (reboot recommended).
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t beginLoadingAnimation(void)
{
    StrobeTaskCommand strobeCommand = {
        .ledNum = NORTH_LED_NUM,
        .initScale = 0xF0,
        .initStrobeUp = false,
        .maxScale = 0xF0,
        .minScale = 0x08,
        .stepSizeHigh = 0x30,
        .stepSizeLow = 0x0C,
        .stepCutoff = 0x30 // stepLow when decreasing, stepHigh when increasing
    };
    esp_err_t err;
    ESP_LOGI(TAG, "Starting loading animation...");

    err = matSetColor(NORTH_LED_NUM, CONFIG_WHITE_RED_COMPONENT + 0x20, CONFIG_WHITE_GREEN_COMPONENT, CONFIG_WHITE_BLUE_COMPONENT);
    if (err == APP_ERR_INVALID_PAGE) return ESP_FAIL;
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;
    err = matSetColor(EAST_LED_NUM, CONFIG_WHITE_RED_COMPONENT + 0x20, CONFIG_WHITE_GREEN_COMPONENT, CONFIG_WHITE_BLUE_COMPONENT);
    if (err == APP_ERR_INVALID_PAGE) return ESP_FAIL;
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;
    err = matSetColor(SOUTH_LED_NUM, CONFIG_WHITE_RED_COMPONENT + 0x20, CONFIG_WHITE_GREEN_COMPONENT, CONFIG_WHITE_BLUE_COMPONENT);
    if (err == APP_ERR_INVALID_PAGE) return ESP_FAIL;
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;
    err = matSetColor(WEST_LED_NUM, CONFIG_WHITE_RED_COMPONENT + 0x20, CONFIG_WHITE_GREEN_COMPONENT, CONFIG_WHITE_BLUE_COMPONENT);
    if (err == APP_ERR_INVALID_PAGE) return ESP_FAIL;
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    err = pauseStrobeRegisterLEDs(portMAX_DELAY);
    if (err != ESP_OK) return err;
    
    err = strobeRegisterLED(strobeCommand);
    if (err != ESP_OK) return err;
    
    strobeCommand.ledNum = EAST_LED_NUM;
    strobeCommand.initScale = 0x30;
    strobeCommand.initStrobeUp = true;
    err = strobeRegisterLED(strobeCommand);
    if (err != ESP_OK)
    {
        if (strobeUnregisterAll() != ESP_OK) ESP_LOGE(TAG, "Failed to unregister strobing!");
        return err;
    }

    strobeCommand.ledNum = SOUTH_LED_NUM;
    strobeCommand.initScale = 0x00;
    strobeCommand.initStrobeUp = true;
    err = strobeRegisterLED(strobeCommand);
    if (err != ESP_OK)
    {
        if (strobeUnregisterAll() != ESP_OK) ESP_LOGE(TAG, "Failed to unregister strobing!");
        return err;
    }

    strobeCommand.ledNum = WEST_LED_NUM;
    strobeCommand.initScale = 0x30;
    strobeCommand.initStrobeUp = false;
    err = strobeRegisterLED(strobeCommand);
    if (err != ESP_OK)
    {
        if (strobeUnregisterAll() != ESP_OK) ESP_LOGE(TAG, "Failed to unregister strobing!");
        return err;
    }

    err = resumeStrobeRegisterLEDs();
    if (err != ESP_OK) return err;

    return ESP_OK;
}

/**
 * @brief Ends the loading animation by unregistering strobing on direction LEDs
 * and setting scaling to normal.
 * 
 * @requires:
 * - strobe task created.
 * - led_matrix component initialized.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if any requirement is unmet.
 * ESP_ERR_TIMEOUT if an I2C transaction failed.
 * ESP_ERR_INVALID_RESPONSE if a matrix page could not be set.
 * APP_ERR_MUTEX_RELEASE if a mutex failed to release (reboot recommended).
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t endLoadingAnimation(void)
{
    esp_err_t err;
    ESP_LOGI(TAG, "Ending loading animation...");
    err = pauseStrobeRegisterLEDs(portMAX_DELAY);
    if (err != ESP_OK) return err;

    err = strobeUnregisterLED(NORTH_LED_NUM);
    if (err != ESP_OK) return err;
    err = strobeUnregisterLED(EAST_LED_NUM);
    if (err != ESP_OK) return err;
    err = strobeUnregisterLED(SOUTH_LED_NUM);
    if (err != ESP_OK) return err;
    err = strobeUnregisterLED(WEST_LED_NUM);
    if (err != ESP_OK) return err;

    err = resumeStrobeRegisterLEDs();
    if (err != ESP_OK) return err;

    /* wait for commands to be processed to avoid race conditions */
    err = strobeWaitQueueProcessed(portMAX_DELAY);
    if (err != ESP_OK) THROW_ERR(ESP_FAIL); // ESP_ERR_TIMEOUT (not thrown)

    /* set scaling to regular value */
    err = matSetScaling(NORTH_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err == APP_ERR_INVALID_PAGE) return ESP_FAIL;
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;
    err = matSetScaling(EAST_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err == APP_ERR_INVALID_PAGE) return ESP_FAIL;
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;
    err = matSetScaling(SOUTH_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err == APP_ERR_INVALID_PAGE) return ESP_FAIL;
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;
    err = matSetScaling(WEST_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err == APP_ERR_INVALID_PAGE) return ESP_FAIL;
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    return ESP_OK;
}

#else
#error "Unsupported hardware version!"
#endif