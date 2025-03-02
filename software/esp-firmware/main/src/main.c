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
#include "app_errors.h"

/* Component includes */
#include "dots_commands.h"
#include "led_registers.h"

#define TAG "app_main"

/** @brief The queue size in elements of the I2C command queue. */
#define I2C_QUEUE_SIZE 20

/** @brief The queue size in elements of the worker command queue. */
#define WORKER_QUEUE_SIZE 1

struct MainTaskResources {
  QueueHandle_t workerQueue;
  nvs_handle_t nvsHandle;
  UserSettings *settings;
  esp_timer_handle_t refreshTimer;
  ErrorResources *errRes;
};

typedef struct MainTaskResources MainTaskResources;

struct MainTaskState {
  bool toggle; // whether the direction of flow should be switched
  bool first; // whether this is the first refresh
  Direction dir; // current LED refresh direction
};

typedef struct MainTaskState MainTaskState;

void initializeApplication(MainTaskResources *res, MainTaskState *state) {
  /* global resources */
  static ErrorResources errRes;
  static UserSettings settings;
  QueueHandle_t I2CQueue;
  QueueHandle_t workerQueue;
  /* input guards */
  SPIN_IF_FALSE(
    (res != NULL &&
     state != NULL),
    NULL
  );
  /* install UART driver */
  ESP_LOGI(TAG, "Installing UART driver...");
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
  /* initialize error handling resources */
  ESP_LOGI(TAG, "Initializing error handling resources...");
  errRes.err = NO_ERR;
  errRes.errTimer = NULL;
  errRes.errMutex = xSemaphoreCreateMutex();
  SPIN_IF_FALSE(
    (errRes.errMutex != NULL),
    NULL
  );
  res->errRes = &errRes;
  /* initialize global resources */
  ESP_LOGI(TAG, "Initializing global resources...");
  I2CQueue = xQueueCreate(I2C_QUEUE_SIZE, sizeof(I2CCommand));
  workerQueue = xQueueCreate(WORKER_QUEUE_SIZE, sizeof(WorkerCommand));
  SPIN_IF_FALSE(
    (I2CQueue != NULL &&
     workerQueue != NULL),
    res->errRes
  );
  res->workerQueue = workerQueue;
  settings.wifiSSID = NULL;
  settings.wifiSSIDLen = 0;
  settings.wifiPass = NULL;
  settings.wifiPassLen = 0;
  res->settings = &settings;
  /* initialize status and direction LEDs */
  ESP_LOGI(TAG, "Initializing status LEDs...");
  SPIN_IF_FALSE(
    ( gpio_set_level(WIFI_LED_PIN, 0) == ESP_OK &&
      gpio_set_level(ERR_LED_PIN, 0) == ESP_OK &&
      gpio_set_level(LED_NORTH_PIN, 0) == ESP_OK &&
      gpio_set_level(LED_EAST_PIN, 0) == ESP_OK &&
      gpio_set_level(LED_SOUTH_PIN, 0) == ESP_OK &&
      gpio_set_level(LED_WEST_PIN, 0) == ESP_OK
    ),
    res->errRes
  );
  SPIN_IF_FALSE(
    ( gpio_set_direction(WIFI_LED_PIN, GPIO_MODE_OUTPUT) == ESP_OK &&
      gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT) == ESP_OK &&
      gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT) == ESP_OK &&
      gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT) == ESP_OK &&
      gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT) == ESP_OK &&
      gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT) == ESP_OK
    ),
    res->errRes
  );
  /* initialize NVS */
  ESP_LOGI(TAG, "Initializing non-volatile storage...");
  SPIN_IF_ERR(
    nvs_flash_init(),
    res->errRes
  );
  SPIN_IF_ERR(
    nvs_open("main", NVS_READWRITE, &(res->nvsHandle)),
    res->errRes
  );
  /* Remove unnecessary NVS entries */
  ESP_LOGI(TAG, "Removing unnecessary nvs entries...");
  SPIN_IF_ERR(
    removeExtraMainNvsEntries(res->nvsHandle),
    res->errRes
  );
  /* Ensure NVS entries exist */
  ESP_LOGI(TAG, "Checking whether nvs entries exist...");
  UPDATE_SETTINGS_IF_ERR(
    nvsEntriesExist(res->nvsHandle),
    res->nvsHandle, res->errRes
  );
  /* Check settings update button (dir button held on startup) */
  ESP_LOGI(TAG, "Checking change settings button...");
  SPIN_IF_ERR(
    gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT), // pin has external pullup
    res->errRes
  );
  if (gpio_get_level(T_SW_PIN) == 0) {
    updateNvsSettings(res->nvsHandle, res->errRes);
  }
  /* retrieve nvs settings */
  ESP_LOGI(TAG, "Retrieving NVS entries...");
  UPDATE_SETTINGS_IF_ERR(
    retrieveNvsEntries(res->nvsHandle, res->settings),
    res->nvsHandle, res->errRes
  );
  /* initialize tcp/ip stack */
  ESP_LOGI(TAG, "Initializing TCP/IP stack...");
  SPIN_IF_ERR(
    esp_netif_init(),
    res->errRes
  );
  SPIN_IF_ERR(
    esp_event_loop_create_default(),
    res->errRes
  );
  esp_netif_create_default_wifi_sta();
  /* Establish wifi connection & tls */
  ESP_LOGI(TAG, "Establishing wifi connection...");
  wifi_init_config_t default_wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  SPIN_IF_ERR(
    esp_wifi_init(&default_wifi_cfg),
    res->errRes
  );
  SPIN_IF_ERR(
    gpio_set_direction(WIFI_LED_PIN, GPIO_MODE_OUTPUT),
    res->errRes
  );
  SPIN_IF_ERR( // allows use of establishWifiConnection and isWifiConnected
    initWifi(res->settings->wifiSSID, res->settings->wifiPass, WIFI_LED_PIN), 
    res->errRes
  );
  establishWifiConnection(); // do not do anything if this returns an error
  esp_tls_t *tls = esp_tls_init();
  SPIN_IF_FALSE(
    (tls != NULL),
    res->errRes
  );
  /* create tasks */
  ESP_LOGI(TAG, "Creating tasks...");
  SPIN_IF_ERR(
    createI2CGatekeeperTask(NULL, I2CQueue),
    res->errRes
  );
  SPIN_IF_ERR(
    createWorkerTask(NULL, workerQueue, I2CQueue, res->errRes),
    res->errRes
  );
  TaskHandle_t otaTask;
  SPIN_IF_ERR(
    createOTATask(&otaTask, res->errRes),
    res->errRes
  );
  /* create refresh timer */
  state->toggle = false;
  res->refreshTimer = createRefreshTimer(xTaskGetCurrentTaskHandle(), &(state->toggle));
  SPIN_IF_FALSE(
    (res->refreshTimer != NULL),
    res->errRes
  );
  /* initialize buttons */
  SPIN_IF_ERR(
    gpio_install_isr_service(0),
    res->errRes
  );
  SPIN_IF_ERR(
    initIOButton(otaTask),
    res->errRes
  );
  SPIN_IF_ERR(
    initDirectionButton(&(state->toggle)),
    res->errRes
  );
}

/**
 * @brief The entrypoint task of the application.
 * 
 * Initializes other tasks, requests user settings, then handles button presses
 * for LED refreshes. As this task handles user input, it should not do a lot
 * of processing. Rather, heavy processing is primarily left to vDotWorkerTask.
 */
void app_main(void)
{
    MainTaskResources res;
    MainTaskState state;
    /* set task priority */
    vTaskPrioritySet(NULL, CONFIG_MAIN_PRIO);
    /* print firmware information */
    ESP_LOGE(TAG, "Traffic Firmware " VERBOSE_VERSION_STR);
    ESP_LOGE(TAG, "OTA binary: " FIRMWARE_UPGRADE_URL);
    ESP_LOGE(TAG, "Max LED number: %d", MAX_NUM_LEDS_REG - 1); // 0 is not an LED
    /* initialize main task state */
    state.toggle = false;
    state.first = true;
#ifdef CONFIG_FIRST_DIR_NORTH
    state.dir = NORTH;
#else
    state.dir = SOUTH;
#endif
    /* initialize application */
    ESP_LOGI(TAG, "1");
    initializeApplication(&res, &state);
    /* quick clear LEDs (potentially leftover from restart) */
    SPIN_IF_ERR(
      quickClearLEDs(res.workerQueue),
      res.errRes
    );
    ESP_LOGI(TAG, "initialization complete, handling toggle button presses...");
    /* handle requests to update all LEDs */
    do {
      /* update leds */
      if (!state.first) {
        if (clearLEDs(res.workerQueue, state.dir) != ESP_OK) {
          ESP_LOGE(TAG, "failed to clear LEDs");
          continue;
        }
      } else {
        state.first = false;
      }
      if (updateLEDs(res.workerQueue, state.dir) != ESP_OK) {
        ESP_LOGE(TAG, "failed to update LEDs");
        continue;
      }
      SPIN_IF_ERR(
        clearLEDs(res.workerQueue, state.dir),
        res.errRes
      );
      SPIN_IF_ERR(
        updateLEDs(res.workerQueue, state.dir),
        res.errRes
      );
      /* set or restart timer */
      esp_err_t err = esp_timer_restart(res.refreshTimer, ((uint64_t) CONFIG_LED_REFRESH_PERIOD) * 60 * 1000000); // restart timer if toggle is pressed
      if (err == ESP_ERR_INVALID_STATE) { // meaning: timer has not yet started
        err = esp_timer_start_periodic(res.refreshTimer, ((uint64_t) CONFIG_LED_REFRESH_PERIOD) * 60 * 1000000);
      }
      SPIN_IF_ERR(
        err,
        res.errRes
      );
      /* wait for button press or timer reset */
      SPIN_IF_ERR(
        enableDirectionButtonIntr(),
        res.errRes
      );
      uint32_t notificationValue;
      do {
        notificationValue = ulTaskNotifyTake(pdTRUE, INT_MAX);
      } while (notificationValue != 1); // a timeout occurred while waiting for button press
      SPIN_IF_ERR(
        disableDirectionButtonIntr(),
        res.errRes
      );
      SPIN_IF_ERR(
        esp_timer_stop(res.refreshTimer),
        res.errRes
      );
      if (state.toggle) {
        state.toggle = false;
        switch (state.dir) {
          case NORTH:
            state.dir = SOUTH;
            break;
          case SOUTH:
            state.dir = NORTH;
            break;
          default:
            state.dir = NORTH;
            break;
        }
      }
    } while (true);
    /* This task has nothing left to do, but should not exit */
    throwFatalError(res.errRes, false);
    for (;;) {
      vTaskDelay(INT_MAX);
    }
}

