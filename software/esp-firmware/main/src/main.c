/**
 * main.c
 * 
 * This file contains the entrypoint task
 * for the application (app_main) and
 * configuration including function hooks.
 */


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
#include "esp_check.h"

#include "app_errors.h"
#include "led_matrix.h"
#include "led_registers.h"
#include "pinout.h"
#include "animations.h"

#include "utilities.h"
#include "tasks.h"
#include "wifi.h"
#include "routines.h"
#include "main_types.h"
#include "refresh.h"

#define TAG "app_main"

/* TomTom HTTPS configuration */
#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE


struct MainTaskResources {
  esp_http_client_handle_t client;
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

void initializeMainState(MainTaskState *state) {
  state->toggle = false;
  state->first = true;

#ifdef CONFIG_FIRST_DIR_NORTH
  state->dir = NORTH;
#else
  state->dir = SOUTH;
#endif
}

void initializeApplication(MainTaskState *state, MainTaskResources *res) {
  /* global resources */
  static ErrorResources errRes;
  static UserSettings settings;
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
  /* Initialize I2C bus and matrices */
  SPIN_IF_ERR(
    matInitialize(I2C_PORT, SDA_PIN, SCL_PIN),
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
  SPIN_IF_ERR(
    removeExtraWorkerNvsEntries(),
    res->errRes
  )
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

esp_http_client_handle_t initHttpClient(void) {
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

esp_err_t updateStatus(Direction dir) {
  esp_err_t ret = ESP_OK;
  int northLvl, southLvl, eastLvl, westLvl;
  /* update direction LEDs */
  switch (dir) {
    case NORTH:
      northLvl = 1;
      eastLvl = 0;
      southLvl = 0;
      westLvl = 1;
      break;
    case SOUTH:
      northLvl = 0;
      eastLvl = 1;
      southLvl = 1;
      westLvl = 0;
      break;
    default:
      goto error_with_dir_leds_off;
      break;
  }
  if (gpio_set_level(LED_NORTH_PIN, northLvl) != ESP_OK) {
    goto error_with_dir_leds_off;
  }
  if (gpio_set_level(LED_EAST_PIN, eastLvl) != ESP_OK) {
    goto error_with_dir_leds_off;
  }
  if (gpio_set_level(LED_SOUTH_PIN, southLvl) != ESP_OK) {
    goto error_with_dir_leds_off;
  }
  if (gpio_set_level(LED_WEST_PIN, westLvl) != ESP_OK) {
    goto error_with_dir_leds_off;
  }
  return ret;
error_with_dir_leds_off:
  ESP_ERROR_CHECK(gpio_set_level(LED_NORTH_PIN, 0));
  ESP_ERROR_CHECK(gpio_set_level(LED_EAST_PIN, 0));
  ESP_ERROR_CHECK(gpio_set_level(LED_SOUTH_PIN, 0));
  ESP_ERROR_CHECK(gpio_set_level(LED_WEST_PIN, 0));
  return ret;
}

esp_err_t mainRefresh(MainTaskState *state, MainTaskResources *res, LEDData typicalNorth[static MAX_NUM_LEDS_REG + 1], LEDData typicalSouth[static MAX_NUM_LEDS_REG + 1]) {
  esp_err_t err;
  LEDData *typicalData;
  LEDData currentSpeeds[MAX_NUM_LEDS_REG + 1];
  /* input guards */
  SPIN_IF_FALSE(res != NULL, NULL);
  if (state == NULL ||
      typicalNorth == NULL ||
      typicalSouth == NULL)
  {
    SPIN_IF_ERR(ESP_FAIL, res->errRes);
  }
  /* handle toggle button press */
  if (state->toggle) {
    state->toggle = false;
    switch (state->dir) {
      case NORTH:
        state->dir = SOUTH;
        break;
      case SOUTH:
        state->dir = NORTH;
        break;
      default:
        state->dir = NORTH;
        break;
    }
  }
  /* determine correct typical LED array */
  switch (state->dir) {
    case NORTH:
      typicalData = typicalNorth;
      break;
    case SOUTH:
      typicalData = typicalSouth;
      break;
    default:
      state->dir = NORTH;
      typicalData = typicalNorth;
      break;
  }
  /* retrieve updated data */
  err = refreshData(currentSpeeds, res->client, state->dir, LIVE, res->errRes);
  SPIN_IF_ERR(err, res->errRes);
  /* refresh LEDs */
  err = updateStatus(state->dir);
  SPIN_IF_ERR(err, res->errRes);
  switch (state->dir)
  {
    case NORTH:
      err = refreshBoard(currentSpeeds, typicalNorth, DIAG_LINE_REVERSE);
      break;
    case SOUTH:
      err = refreshBoard(currentSpeeds, typicalSouth, DIAG_LINE);
      break;
    default:
      state->dir = NORTH;
      err = refreshBoard(currentSpeeds, typicalNorth, DIAG_LINE);
      break;
  }
  SPIN_IF_FALSE((err == ESP_OK || err == REFRESH_ABORT), res->errRes);
  return err;
}

void waitForTaskNotification(MainTaskResources *res) {
  esp_err_t err = esp_timer_restart(res->refreshTimer, ((uint64_t) CONFIG_LED_REFRESH_PERIOD) * 60 * 1000000); // restart timer if toggle is pressed
  if (err == ESP_ERR_INVALID_STATE) { // meaning: timer has not yet started
    err = esp_timer_start_periodic(res->refreshTimer, ((uint64_t) CONFIG_LED_REFRESH_PERIOD) * 60 * 1000000);
  }
  SPIN_IF_ERR(
    err,
    res->errRes
  );
  /* wait for button press or timer reset */
  uint32_t notificationValue;
  do {
    notificationValue = ulTaskNotifyTake(pdTRUE, INT_MAX);
  } while (notificationValue != 1); // a timeout occurred while waiting for button press
  SPIN_IF_ERR(
    esp_timer_stop(res->refreshTimer),
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
    esp_err_t err;
    LEDData typicalNorthSpeeds[MAX_NUM_LEDS_REG + 1];
    LEDData typicalSouthSpeeds[MAX_NUM_LEDS_REG + 1];
    /* set task priority */
    vTaskPrioritySet(NULL, CONFIG_MAIN_PRIO);
    /* print firmware information */
    ESP_LOGE(TAG, "Traffic Firmware " VERBOSE_VERSION_STR);
    ESP_LOGE(TAG, "OTA binary: " FIRMWARE_UPGRADE_URL);
    ESP_LOGE(TAG, "Max LED number: %d", MAX_NUM_LEDS_REG); // 0 is not an LED
    /* initialize application */
    initializeMainState(&state);
    initializeApplication(&state, &res);
    if (matReset() != ESP_OK ||
        matSetGlobalCurrentControl(CONFIG_GLOBAL_LED_CURRENT) != ESP_OK ||
        matSetOperatingMode(NORMAL_OPERATION) != ESP_OK) 
    {
        ESP_LOGE(TAG, "failed to quick clear matrices");
    }
    /* quick clear LEDs, maybe leftover from reboot */
    err = quickClearBoard();
    SPIN_IF_ERR(err, res.errRes);
    /* retrieve speeds */
    res.client = initHttpClient();
    err = refreshData(typicalNorthSpeeds, res.client, NORTH, TYPICAL, res.errRes);
    SPIN_IF_ERR(err, res.errRes);
    err = refreshData(typicalSouthSpeeds, res.client, SOUTH, TYPICAL, res.errRes);
    SPIN_IF_ERR(err, res.errRes);
    ESP_LOGI(TAG, "initialization complete, handling toggle button presses...");
    /* handle requests to update all LEDs */
    err = enableDirectionButtonIntr();
    SPIN_IF_ERR(err, res.errRes);
    while (true) {
      err = mainRefresh(&state, &res, typicalNorthSpeeds, typicalSouthSpeeds);
      waitForTaskNotification(&res);
      if (err == REFRESH_ABORT)
      {
        err = quickClearBoard();
        SPIN_IF_ERR(err, res.errRes);
      } else {
        clearBoard(state.dir);
      }
    }
    /* This task has nothing left to do, but should not exit */
    throwFatalError(res.errRes, false);
    for (;;) {
      vTaskDelay(INT_MAX);
    }
}