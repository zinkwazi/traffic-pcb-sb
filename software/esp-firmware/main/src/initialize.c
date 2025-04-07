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
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_vfs_dev.h"
#include "esp_wifi_default.h"
#include "esp_wifi.h"
#include "freertos/freeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "led_matrix.h"
#include "pinout.h"

#include "main_types.h"
#include "nvs_settings.h"
#include "ota.h"
#include "utilities.h"
#include "strobe_task.h"
#include "routines.h"
#include "wifi.h"

#define TAG "init"

/* TomTom HTTPS configuration */
#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE

#define USB_SERIAL_BUF_SIZE (1024)

static void initializeMainState(MainTaskState *state);
static esp_http_client_handle_t initHttpClient(void);


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
    static ErrorResources errRes;
    static UserSettings settings;

    /* local variables */
    esp_err_t err = ESP_FAIL;
    esp_tls_t *tls = NULL;
    TaskHandle_t otaTask = NULL;
    wifi_init_config_t default_wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    nvs_handle_t workerHandle;

    /* input guards */
    if (state == NULL)
        return ESP_ERR_INVALID_ARG;
    if (res == NULL)
        return ESP_ERR_INVALID_ARG;

    /* initialize state and resources to known values */
    initializeMainState(state);
    res->client = NULL;
    res->nvsHandle = (nvs_handle_t)NULL;
    res->refreshTimer = NULL;

    /* initialize global resources */
    errRes.err = NO_ERR;
    errRes.errTimer = NULL;
    errRes.errMutex = xSemaphoreCreateMutex();
    if (errRes.errMutex == NULL)
        return ESP_ERR_NO_MEM;
    res->errRes = &errRes;

    settings.wifiSSID = NULL;
    settings.wifiSSIDLen = 0;
    settings.wifiPass = NULL;
    settings.wifiPassLen = 0;
    res->settings = &settings;

    /* initialize and cleanup non-volatile storage */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_LOGE(TAG, "no free pages in nvs, need to erase nvs partition with parttool.py.");
    }
    FATAL_IF_ERR(err, res->errRes);

    res->nvsHandle = openMainNvs();
    FATAL_IF_FALSE((res->nvsHandle != (nvs_handle_t) NULL), res->errRes);
    err = removeExtraMainNvsEntries(res->nvsHandle); // keep handle open
    FATAL_IF_ERR(err, res->errRes);

    workerHandle = openWorkerNvs();
    FATAL_IF_FALSE((workerHandle != (nvs_handle_t) NULL), res->errRes);
    err = removeExtraWorkerNvsEntries(workerHandle); // keep handle open
    FATAL_IF_ERR(err, res->errRes);

    /* check if a settings update is requested or necessary */
    err = gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT); // pin has external pullup
    FATAL_IF_ERR(err, res->errRes);
    err = retrieveNvsEntries(res->nvsHandle, &settings);
    if (gpio_get_level(T_SW_PIN) == 0 || err != ESP_OK)
    {
        ESP_LOGI(TAG, "updating settings, err: %d", err);
        updateNvsSettings(res->nvsHandle, res->errRes);
    }

    /* retrieve nvs settings */
    err = nvsEntriesExist(res->nvsHandle);
    FATAL_IF_ERR(err, res->errRes);
    err = retrieveNvsEntries(res->nvsHandle, &settings);
    FATAL_IF_ERR(err, res->errRes);

    /* initialize tcp/ip stack */
    err = esp_netif_init();
    FATAL_IF_ERR(err, res->errRes);
    err = esp_event_loop_create_default();
    FATAL_IF_ERR(err, res->errRes);
    (void) esp_netif_create_default_wifi_sta(); // don't need result

    /* establish wifi connection & tls */
    err = esp_wifi_init(&default_wifi_cfg);
    FATAL_IF_ERR(err, res->errRes);
    err = initWifi(res->settings->wifiSSID, res->settings->wifiPass);
    FATAL_IF_ERR(err, res->errRes);
    err = establishWifiConnection();
    while (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
        /* nvs does not have enough space for wifi. I conclude this is due
        to fragmentation, so simply erasing everything is a good enough fix.
        This should really only occur when the user has just changed wifi
        settings, so I don't think there is a risk of killing non-wifi 
        operation here by deleting stored data */
        ESP_LOGE(TAG, "erasing nvs");
        err = nvs_erase_all(res->nvsHandle); // keep handle open
        if (err != ESP_OK)
        {
            FATAL_IF_ERR(err, res->errRes);
        }
        err = nvs_erase_all(workerHandle); // close handle
        if (err != ESP_OK)
        {
            FATAL_IF_ERR(err, res->errRes);
        }

        ESP_LOGE(TAG, "rewriting user settings to nvs");
        err = storeNvsSettings(res->nvsHandle, *res->settings);
        FATAL_IF_ERR(err, res->errRes);

        esp_restart();
    } // ignore other error codes

    tls = esp_tls_init();
    if (tls == NULL)
        return ESP_ERR_NO_MEM;

    /* initialize http client */
    res->client = initHttpClient();
    if (res->client == NULL)
        return ESP_FAIL;

    /* create tasks */
    err = createStrobeTask(NULL, res->errRes);
    FATAL_IF_ERR(err, res->errRes);
    err = createOTATask(&otaTask, res->errRes);
    FATAL_IF_ERR(err, res->errRes);
    if (otaTask == NULL)
        return ESP_FAIL;

    /* create refresh timer */
    state->toggle = false;
    res->refreshTimer = createRefreshTimer(xTaskGetCurrentTaskHandle(), &(state->toggle));
    if (res->refreshTimer == NULL)
        return ESP_FAIL;

    /* initialize buttons */
    err = gpio_install_isr_service(0);
    FATAL_IF_ERR(err, res->errRes);
    err = initIOButton(otaTask);
    FATAL_IF_ERR(err, res->errRes);
    err = initDirectionButton(&(state->toggle));
    FATAL_IF_ERR(err, res->errRes);

    return ESP_OK;
}

#if CONFIG_HARDWARE_VERSION == 1

/**
 * @brief Initializes I2C bus to communicate with LED matrices.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initializeMatrices(void)
{
    esp_err_t err;
    err = matInitialize(I2C_PORT, SDA_PIN, SCL_PIN);
    return err;
}

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
 * @brief Initializes I2C buses to communicate with LED matrices.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initializeMatrices(void)
{
    esp_err_t err;

    err = matInitializeBus1(I2C1_PORT, SDA1_PIN, SCL1_PIN);
    if (err != ESP_OK)
        return err;
    err = matInitializeBus2(I2C2_PORT, SDA2_PIN, SCL2_PIN);
    err = matReset();
    if (err != ESP_OK) return err;
    err = matSetGlobalCurrentControl(CONFIG_GLOBAL_LED_CURRENT);
    if (err != ESP_OK) return err;
    err = matSetOperatingMode(NORMAL_OPERATION);
    return err;
}

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
 * - initializeMatrices executed.
 *
 * @returns ESP_OK if successful.
 */
esp_err_t initializeIndicatorLEDs(void)
{
    esp_err_t err;
    err = matSetScaling(OTA_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(OTA_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK)
        return err;
    err = matSetScaling(WIFI_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(WIFI_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK)
        return err;
    err = matSetScaling(ERROR_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(ERROR_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK)
        return err;
    err = matSetScaling(NORTH_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(NORTH_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK)
        return err;
    err = matSetScaling(EAST_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(EAST_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK)
        return err;
    err = matSetScaling(WEST_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(WEST_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK)
        return err;
    err = matSetScaling(SOUTH_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK)
        return err;
    err = matSetColor(SOUTH_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK)
        return err;
    err = initLEDLegendLight(FAST_RED, FAST_GREEN, FAST_BLUE);
    if (err != ESP_OK)
        return err;
    err = initLEDLegendMedium(MEDIUM_RED, MEDIUM_GREEN, MEDIUM_BLUE);
    if (err != ESP_OK)
        return err;
    err = initLEDLegendHeavy(SLOW_RED, SLOW_GREEN, SLOW_BLUE);
    if (err != ESP_OK)
        return err;
    return err;
}

/**
 * @brief Initializes the Heavy LED in the legend to the provided color.
 *
 * @requires:
 * - initializeMatrices executed.
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
 * - initializeMatrices executed.
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
 * - initializeMatrices executed.
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
 * @brief Initializes an http client to the server.
 *
 * @note Returned client must have a call to esp_http_client_cleanup after use.
 *
 * @returns A handle to the initialized client if successful,
 *          otherwise NULL.
 */
static esp_http_client_handle_t initHttpClient(void)
{
    esp_http_client_config_t httpConfig = {
        .host = CONFIG_DATA_SERVER,
        .path = "/",
        .auth_type = API_AUTH_TYPE,
        .method = API_METHOD,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = NULL,
        .user_data = NULL,
    };

    return esp_http_client_init(&httpConfig);
}

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