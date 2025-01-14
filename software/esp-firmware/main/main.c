/**
 * main.c
 * 
 * This file contains the entrypoint task
 * for the application (app_main) and
 * configuration including function hooks.
 */



/* IDF component includes */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_vfs_dev.h"
#include "driver/uart_vfs.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_tls.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_timer.h"
#include "nvs.h"
#include "esp_https_ota.h"

/* Main component includes */
#include "pinout.h"
#include "tasks.h"
#include "utilities.h"
#include "wifi.h"
#include "routines.h"
#include "main_types.h"

/* Component includes */
#include "dots_commands.h"
#include "led_registers.h"

#define TAG "app_main"

/**
 * @brief The entrypoint task of the application.
 * 
 * Initializes other tasks, requests user settings, then handles button presses
 * for LED refreshes. As this task handles user input, it should not do a lot
 * of processing. Rather, heavy processing is primarily left to vDotWorkerTask.
 */
void app_main(void)
{
    QueueHandle_t I2CQueue, dotQueue;
    nvs_handle_t nvsHandle;
    struct UserSettings settings;
    /* set task priority */
    vTaskPrioritySet(NULL, MAIN_TASK_PRIO);
    /* print firmware information */
    ESP_LOGE(TAG, "Traffic Firmware " CONFIG_HARDWARE_VERSION CONFIG_FIRMWARE_VERSION CONFIG_FIRMWARE_CONF);
    ESP_LOGE(TAG, "OTA binary: " CONFIG_FIRMWARE_UPGRADE_SERVER "/firmware/firmware" CONFIG_HARDWARE_VERSION ".bin");
    /* install UART driver */
    ESP_LOGI(TAG, "Installing UART driver");
    SPIN_IF_ERR(
      uart_driver_install(UART_NUM_0,
                          UART_HW_FIFO_LEN(UART_NUM_0) + 16, 
                          UART_HW_FIFO_LEN(UART_NUM_0) + 16, 
                          32, 
                          NULL, 
                          0),
      NULL
    );
    uart_vfs_dev_use_driver(UART_NUM_0); // enable interrupt driven IO
    /* set direction of direction led pins */
    gpio_set_level(LED_NORTH_PIN, 0);
    gpio_set_level(LED_EAST_PIN, 0);
    gpio_set_level(LED_SOUTH_PIN, 0);
    gpio_set_level(LED_WEST_PIN, 0);
    SPIN_IF_ERR(
      gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT),
      NULL
    );
    SPIN_IF_ERR(
      gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT),
      NULL
    );
    SPIN_IF_ERR(
      gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT),
      NULL
    );
    SPIN_IF_ERR(
      gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT),
      NULL
    );
    /* Create error handling synchronization variables */
    static ErrorResources errRes = {
      .err = NO_ERR,
      .errTimer = NULL,
      .errMutex = NULL,
    };
    errRes.err = NO_ERR;
    errRes.errTimer = NULL;
    errRes.errMutex = xSemaphoreCreateMutex();
    SPIN_IF_FALSE(
      (errRes.errMutex != NULL),
      NULL
    );
    /* initialize NVS */
    ESP_LOGI(TAG, "initializing nvs");
    SPIN_IF_ERR(
      nvs_flash_init(),
      &errRes
    );
    SPIN_IF_ERR(
      nvs_open("main", NVS_READWRITE, &nvsHandle),
      &errRes
    );
    /* Remove unnecessary NVS entries */
    ESP_LOGI(TAG, "removing unnecessary nvs entries");
    SPIN_IF_ERR(
      removeExtraMainNvsEntries(nvsHandle),
      &errRes
    );
    /* Ensure NVS entries exist */
    ESP_LOGI(TAG, "checking whether nvs entries exist");
    UPDATE_SETTINGS_IF_ERR(
      nvsEntriesExist(nvsHandle),
      nvsHandle, &errRes
    );
    /* Check manual settings update button (dir button held on startup) */
    ESP_LOGI(TAG, "checking manual change settings button");
    SPIN_IF_ERR(
      gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT), // pin has external pullup
      &errRes
    );
    if (gpio_get_level(T_SW_PIN) == 0) {
      updateNvsSettings(nvsHandle, &errRes);
    }
    /* retrieve nvs settings */
    ESP_LOGI(TAG, "retrieving NVS entries");
    UPDATE_SETTINGS_IF_ERR(
      retrieveNvsEntries(nvsHandle, &settings),
      nvsHandle, &errRes
    );
    /* initialize tcp/ip stack */
    ESP_LOGI(TAG, "initializing TCP/IP stack");
    SPIN_IF_ERR(
      esp_netif_init(),
      &errRes
    );
    SPIN_IF_ERR(
      esp_event_loop_create_default(),
      &errRes
    );
    esp_netif_create_default_wifi_sta();
    /* Establish wifi connection & tls */
    ESP_LOGI(TAG, "establishing wifi connection");
    wifi_init_config_t default_wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    SPIN_IF_ERR(
      esp_wifi_init(&default_wifi_cfg),
      &errRes
    );
    SPIN_IF_ERR(
      gpio_set_direction(WIFI_LED_PIN, GPIO_MODE_OUTPUT),
      &errRes
    );
    SPIN_IF_ERR(
      initWifi(settings.wifiSSID, settings.wifiPass, WIFI_LED_PIN), // allows use of establishWifiConnection and isWifiConnected
      &errRes
    );
    establishWifiConnection(); // do not do anything if this returns an error
    esp_tls_t *tls = esp_tls_init();
    SPIN_IF_FALSE(
      (tls != NULL),
      &errRes
    );
    /* Create queues and event groups */
    I2CQueue = xQueueCreate(I2C_QUEUE_SIZE, sizeof(I2CCommand));
    SPIN_IF_FALSE(
      (I2CQueue != NULL),
      &errRes
    );
    dotQueue = xQueueCreate(DOTS_QUEUE_SIZE, sizeof(WorkerCommand));
    SPIN_IF_FALSE(
      (dotQueue != NULL),
      &errRes
    );
    EventGroupHandle_t workerEvents = xEventGroupCreate();
    SPIN_IF_FALSE(
      (workerEvents != NULL),
      &errRes
    );
    /* create tasks */
    ESP_LOGI(TAG, "creating tasks");
    TaskHandle_t otaTask;
    SPIN_IF_ERR(
      createI2CGatekeeperTask(NULL, I2CQueue, I2C_PORT, SDA_PIN, SCL_PIN),
      &errRes
    );
    SPIN_IF_ERR(
      createWorkerTask(NULL, dotQueue, I2CQueue, &errRes),
      &errRes
    );
    SPIN_IF_ERR(
      createOTATask(&otaTask, &errRes),
      &errRes
    );
    /* initialize pins */
    ESP_LOGI(TAG, "initializing pins");
    SPIN_IF_ERR(
      initDirectionLEDs(),
      &errRes
    );
    /* create timer */
    bool toggle = false;
    TickType_t lastTickISR = 0;
    struct dirButtonISRParams timerParams;
    timerParams.mainTask = xTaskGetCurrentTaskHandle();
    timerParams.lastISR = &lastTickISR;
    timerParams.toggle = &toggle;
    esp_timer_create_args_t timerArgs = {
      .callback = dirButtonISR, // use 'timerCallback' to not trigger direction change (with .arg = xTaskGetCurrentTaskHandle())
      .arg = &timerParams, // timer will send an task notification to this main task
      .dispatch_method = ESP_TIMER_ISR,
      .name = "ledTimer",
    };
    esp_timer_handle_t timer;
    SPIN_IF_ERR(
      esp_timer_create(&timerArgs, &timer),
      &errRes
    );
    /* initialize buttons */
    SPIN_IF_ERR(
      gpio_install_isr_service(0),
      &errRes
    );
    SPIN_IF_ERR(
      initIOButton(otaTask),
      &errRes
    );
    SPIN_IF_ERR(
      initDirectionButton(&lastTickISR, &toggle),
      &errRes
    );
    /* quick clear LEDs */
    SPIN_IF_ERR(
      quickClearLEDs(dotQueue),
      &errRes
    );
    ESP_LOGI(TAG, "initialization complete, handling toggle button presses...");
    /* handle requests to update all LEDs */
    Direction currDirection = SOUTH;
    bool first = true;
    do {
      /* update leds */
      if (!first) {
        if (clearLEDs(dotQueue, currDirection) != ESP_OK) {
          ESP_LOGE(TAG, "failed to clear LEDs");
          continue;
        }
      } else {
        first = false;
      }
      if (updateLEDs(dotQueue, currDirection) != ESP_OK) {
        ESP_LOGE(TAG, "failed to update LEDs");
        continue;
      }
      /* set or restart timer */
      esp_err_t err = esp_timer_restart(timer, ((uint64_t) CONFIG_LED_REFRESH_PERIOD) * 60 * 1000000); // restart timer if toggle is pressed
      if (err == ESP_ERR_INVALID_STATE) { // meaning: timer has not yet started
        err = esp_timer_start_periodic(timer, ((uint64_t) CONFIG_LED_REFRESH_PERIOD) * 60 * 1000000);
      }
      SPIN_IF_ERR(
        err,
        &errRes
      );
      /* wait for button press or timer reset */
      SPIN_IF_ERR(
        enableDirectionButtonIntr(),
        &errRes
      );
      uint32_t notificationValue;
      do {
        notificationValue = ulTaskNotifyTake(pdTRUE, INT_MAX);
      } while (notificationValue != 1); // a timeout occurred while waiting for button press
      SPIN_IF_ERR(
        disableDirectionButtonIntr(),
        &errRes
      );
      SPIN_IF_ERR(
        esp_timer_stop(timer),
        &errRes
      );
      if (toggle) {
        toggle = false;
        switch (currDirection) {
          case NORTH:
            currDirection = SOUTH;
            break;
          case SOUTH:
            currDirection = NORTH;
            break;
          default:
            currDirection = NORTH;
            break;
        }
      }
    } while (true);
    /* This task has nothing left to do, but should not exit */
    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(ERR_LED_PIN, 1);
    for (;;) {
      vTaskDelay(INT_MAX);
    }
}

