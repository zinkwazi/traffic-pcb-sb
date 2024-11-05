/**
 * main.c
 * 
 * This file contains the entrypoint task
 * for the application (app_main) and
 * configuration including function hooks.
 */

/* IDF component includes */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
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

/* Main component includes */
#include "pinout.h"
#include "worker.h"

/* Component includes */
#include "api_config.h"
#include "tomtom.h"
#include "led_locations.h"
#include "dots_commands.h"

#define TAG "app_main"

#define WIFI_SSID_NVS_NAME "wifi_ssid"
#define WIFI_PASS_NVS_NAME "wifi_pass"
#define API_KEY_NVS_NAME "api_key"

#define I2C_GATEKEEPER_STACK ESP_TASK_MAIN_STACK // TODO: determine minimum stack size for i2c gatekeeper
#define I2C_GATEKEEPER_PRIO (ESP_TASK_MAIN_PRIO + 1) // always start an I2C command if possible
#define I2C_QUEUE_SIZE 20

#define NUM_DOT_WORKERS 5
#define DOTS_WORKER_STACK ESP_TASK_MAIN_STACK
#define DOTS_WORKER_PRIO (ESP_TASK_MAIN_PRIO - 1)
#define DOTS_QUEUE_SIZE 10

#define NUM_LEDS sizeof(northLEDLocs) / sizeof(northLEDLocs[0])
#define DOTS_GLOBAL_CURRENT 0x08

#define SPIN_IF_ERR(x) if (x != ESP_OK) { spinForever(); }
#define SPIN_IF_FALSE(x) if (!x) { spinForever(); } 
#define UPDATE_SETTINGS_IF_ERR(x, handle) if (x != ESP_OK) { updateSettingsAndRestart(handle); }
#define UPDATE_SETTINGS_IF_FALSE(x, handle) if (!x) { updateSettingsAndRestart(handle); }

struct userSettings {
  char *wifiSSID;
  size_t wifiSSIDLen;
  char *wifiPass;
  size_t wifiPassLen;
  char *apiKey;
  size_t apiKeyLen;
};

struct dirButtonISRParams {
  TaskHandle_t mainTask; /* a handle to the main task used to send a notification */
  bool *toggle; /* indicates to the main task that the direction button has been 
                   pressed, rather than a notification from an autorefresh timer */ 
};

void dirButtonISR(void *params);
void timerCallback(void *params);
esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle);
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle);
esp_err_t initDotMatrices(QueueHandle_t I2CQueue);
esp_err_t updateLEDs(QueueHandle_t dotQueue, Direction dir);
esp_err_t retrieveNvsEntries(nvs_handle_t nvsHandle, struct userSettings *settings);
esp_err_t createI2CGatekeeperTask(QueueHandle_t I2CQueue);
esp_err_t createDotWorkerTask(char *name, QueueHandle_t dotQueue, QueueHandle_t I2CQueue, char *apiKey);
esp_err_t initDirectionLEDs(void);
esp_err_t initDirectionButton(bool *toggle);
esp_err_t enableDirectionButtonIntr(void);
esp_err_t disableDirectionButtonIntr(void);
esp_err_t clearLEDs(QueueHandle_t I2CQueue, Direction dir);
void spinForever(void);
void updateSettingsAndRestart(nvs_handle_t nvsHandle);

/**
 * The entrypoint task of the application, which initializes 
 * other tasks, requests user settings, then handles button
 * presses for LED refreshes.
 */
void app_main(void)
{
    QueueHandle_t I2CQueue, dotQueue;
    nvs_handle_t nvsHandle;
    struct userSettings settings;
    int i;
    /* install UART driver */
    SPIN_IF_ERR(
      uart_driver_install(UART_NUM_0,
                          UART_HW_FIFO_LEN(UART_NUM_0) + 16, 
                          UART_HW_FIFO_LEN(UART_NUM_0) + 16, 
                          32, 
                          NULL, 
                          0)
    );
    uart_vfs_dev_use_driver(UART_NUM_0); // enable interrupt driven IO
    /* initialize NVS */
    ESP_LOGI(TAG, "initializing nvs");
    SPIN_IF_ERR(
      nvs_flash_init()
    );
    
    /* Ensure NVS entries exist */
    SPIN_IF_ERR(
      nvs_open("main", NVS_READWRITE, &nvsHandle)
    );
    UPDATE_SETTINGS_IF_ERR(
      nvsEntriesExist(nvsHandle), nvsHandle
    );
    /* Check manual settings update button (dir button held on startup) */
    SPIN_IF_ERR(
      gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT) // pin has external pullup
    );
    if (gpio_get_level(T_SW_PIN) == 0) {
      updateSettingsAndRestart(nvsHandle); // updates settings, then restarts
    }
    /* retrieve nvs settings */
    UPDATE_SETTINGS_IF_ERR(
      retrieveNvsEntries(nvsHandle, &settings), 
      nvsHandle
    );

    /* initialize tcp/ip stack */
    ESP_LOGI(TAG, "initializing TCP/IP stack");
    SPIN_IF_ERR(
      esp_netif_init()
    );
    SPIN_IF_ERR(
      esp_event_loop_create_default()
    );
    esp_netif_create_default_wifi_sta();
    /* Establish wifi connection */
    ESP_LOGI(TAG, "establishing wifi connection");
    wifi_init_config_t default_wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    SPIN_IF_ERR(
      esp_wifi_init(&default_wifi_cfg)
    );
    UPDATE_SETTINGS_IF_ERR(
      establishWifiConnection(settings.wifiSSID, settings.wifiPass), nvsHandle
    );
    esp_tls_t *tls = esp_tls_init();
    SPIN_IF_FALSE(
      (tls != NULL)
    );
    /* turn on wifi indicator */
    SPIN_IF_ERR(
      gpio_set_direction(WIFI_LED_PIN, GPIO_MODE_OUTPUT)
    );
    SPIN_IF_ERR(
      gpio_set_level(WIFI_LED_PIN, 1)
    );
    /* Create queues */
    I2CQueue = xQueueCreate(I2C_QUEUE_SIZE, sizeof(I2CCommand));
    SPIN_IF_FALSE(
      (I2CQueue != NULL)
    );
    dotQueue = xQueueCreate(DOTS_QUEUE_SIZE, sizeof(DotCommand));
    SPIN_IF_FALSE(
      (dotQueue != NULL)
    );
    /* create tasks */
    SPIN_IF_ERR(
      createI2CGatekeeperTask(I2CQueue)
    );
    for (i = 0; i < NUM_DOT_WORKERS; i++) {
      SPIN_IF_ERR(
        createDotWorkerTask("dotWorker", dotQueue, I2CQueue, settings.apiKey)
      );
    }
    /* initialize pins */
    SPIN_IF_ERR(
      initDirectionLEDs()
    );
    /* initialize matrices */
    SPIN_IF_ERR(
      initDotMatrices(I2CQueue)
    );
    SPIN_IF_ERR(
      clearLEDs(I2CQueue, SOUTH)
    );
    /* create timer */
    esp_timer_create_args_t timerArgs = {
      .callback = timerCallback,
      .arg = xTaskGetCurrentTaskHandle(), // timer will send an event notification to this main task
      .dispatch_method = ESP_TIMER_ISR,
      .name = "ledTimer",
    };
    esp_timer_handle_t timer;
    SPIN_IF_ERR(
      esp_timer_create(&timerArgs, &timer)
    );
    /* initialize direction button */
    bool toggle = false;
    SPIN_IF_ERR(
      gpio_install_isr_service(0)
    );
    SPIN_IF_ERR(
      initDirectionButton(&toggle)
    );
    ESP_LOGI(TAG, "initialization complete, handling toggle button presses...");
    /* handle requests to update all LEDs */
    
    Direction currDirection = NORTH;
    for (;;) {
      SPIN_IF_ERR(
        enableDirectionButtonIntr()
      );
      uint32_t ulNotificationValue = ulTaskNotifyTake(0, INT_MAX);
      while (ulNotificationValue != 1) { // a timeout occurred while waiting for button press
        continue;
      }
      SPIN_IF_ERR(
        disableDirectionButtonIntr()
      );
      esp_err_t err = esp_timer_restart(timer, CONFIG_LED_REFRESH_PERIOD * 60 * 1000000); // restart timer if toggle is pressed
      if (err == ESP_ERR_INVALID_STATE) { // meaning: timer has not yet started
        err = esp_timer_start_periodic(timer, CONFIG_LED_REFRESH_PERIOD * 60 * 1000000); // don't start refreshing until initial toggle press
      }
      SPIN_IF_ERR(
        err
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
            ESP_LOGW(TAG, "current direction is not NORTH nor SOUTH, setting NORTH...");
            break;
        }
      }
      if (clearLEDs(I2CQueue, currDirection) != ESP_OK) {
        ESP_LOGE(TAG, "failed to clear LEDs");
        continue;
      }
      if (updateLEDs(dotQueue, currDirection) != ESP_OK) {
        ESP_LOGE(TAG, "failed to update LEDs");
        continue;
      }
    }
    /* This task has nothing left to do, but should not exit */
    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(ERR_LED_PIN, 1);
    for (;;) {
      vTaskDelay(INT_MAX);
    }
}

/**
 * Handles direction button presses once the main task is 
 * ready to refresh LEDs. A button press is only acted upon
 * once the main task has refreshed all LEDs because the ISR
 * sends a task notification to the main task, which the task
 * only checks once it has finished handling a previous press.
 */
void dirButtonISR(void *params) {
  TaskHandle_t mainTask = ((struct dirButtonISRParams *) params)->mainTask;
  bool *toggle = ((struct dirButtonISRParams *) params)->toggle;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  *toggle = true;
  vTaskNotifyGiveFromISR(mainTask, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * Periodically sends a task notification to the main task
 * to refresh all LEDs if the direction button has not been
 * pressed. This function does not indicate to the main
 * task that it should toggle the current direction, unlike
 * dirButtonISR(). The timer that calls this function restarts
 * if the direction button has been pressed.
 */
void timerCallback(void *params) {
  TaskHandle_t mainTask = (TaskHandle_t) params;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(mainTask, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * Determines whether user settings currently exist in non-volatile
 * storage, which should not be true on the first powerup of the system.
 */
esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle) {
  esp_err_t ret;
  nvs_type_t nvsType;
  ret = nvs_find_key(nvsHandle, WIFI_SSID_NVS_NAME, &nvsType);
  ESP_RETURN_ON_FALSE(
    (ret == ESP_OK && nvsType == NVS_TYPE_STR), ret,
    TAG, "failed to lookup wifi ssid in non-volatile storage"
  );
  ret = nvs_find_key(nvsHandle, WIFI_PASS_NVS_NAME, &nvsType);
  ESP_RETURN_ON_FALSE(
    (ret == ESP_OK && nvsType == NVS_TYPE_STR), ret,
    TAG, "failed to lookup wifi password in non-volatile storage"
  );
  ret = nvs_find_key(nvsHandle, API_KEY_NVS_NAME, &nvsType);
  ESP_RETURN_ON_FALSE(
    (ret == ESP_OK && nvsType == NVS_TYPE_STR), ret,
    TAG, "failed to lookup API key in non-volatile storage"
  );
  return ret;
}

/**
 * Uses UART0 to query settings from the user, then writes
 * the responses into non-volatile storage.
 */
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle) {
  const unsigned int bufLen = 256;
  char c;
  char buf[bufLen];
  ESP_LOGI(TAG, "could not find NVS entries... Querying from user...");
  printf("\n\nWifi SSID: ");
  fflush(stdout);
  for (int i = 0; i < bufLen; i++) {
    buf[i] = getc(stdin);
    if (buf[i] == '\n') {
      buf[i] = '\0';
      break;
    }
    printf("%c", buf[i]);
    fflush(stdout);
  }
  while ((c = getchar()) != '\n') {}
  buf[bufLen] = '\0'; // in case the user writes too much
  printf("\nYou entered: %s\n", buf);
  fflush(stdout);
  ESP_RETURN_ON_ERROR(
    nvs_set_str(nvsHandle, WIFI_SSID_NVS_NAME, buf),
    TAG, "failed to write wifi SSID to non-volatile storage"
  );
  printf("\n\nWifi Password: ");
  fflush(stdout);
  for (int i = 0; i < bufLen; i++) {
    buf[i] = getc(stdin);
    if (buf[i] == '\n') {
      buf[i] = '\0';
      break;
    }
    printf("%c", buf[i]);
    fflush(stdout);
  }
  while ((c = getchar()) != '\n') {}
  buf[bufLen] = '\0'; // in case the user writes too much
  printf("\nYou entered: %s\n", buf);
  fflush(stdout);
  ESP_RETURN_ON_ERROR(
    nvs_set_str(nvsHandle, WIFI_PASS_NVS_NAME, buf),
    TAG, "failed to write wifi password to non-volatile storage"
  );
  printf("\n\nAPI Key: ");
  fflush(stdout);
  for (int i = 0; i < bufLen; i++) {
    buf[i] = getc(stdin);
    if (buf[i] == '\n') {
      buf[i] = '\0';
      break;
    }
    printf("%c", buf[i]);
    fflush(stdout);
  }
  while ((c = getchar()) != '\n') {}
  buf[bufLen] = '\0'; // in case the user writes too much
  printf("\nYou entered: %s\n", buf);
  fflush(stdout);
  ESP_RETURN_ON_ERROR(
    nvs_set_str(nvsHandle, API_KEY_NVS_NAME, buf),
    TAG, "failed to write API key to non-volatile storage"
  );
  ESP_RETURN_ON_ERROR(
    nvs_commit(nvsHandle),
    TAG, "failed to commit NVS changes"
  );
  return ESP_OK;
}

/**
 * Retrieves user settings from non-volatile storage, placing
 * results in the provided settings struct with space allocated
 * from the heap. It is necessary for the calling function to
 * free pointers in settings if settings is to be destroyed.
 */
esp_err_t retrieveNvsEntries(nvs_handle_t nvsHandle, struct userSettings *settings)
{
  /* retrieve wifi ssid */
  ESP_RETURN_ON_ERROR(
    nvs_get_str(nvsHandle, WIFI_SSID_NVS_NAME, NULL, &(settings->wifiSSIDLen)),
    TAG, "failed to retrieve wifi ssid from nvs"
  );
  if ((settings->wifiSSID = malloc(settings->wifiSSIDLen)) != NULL) {
    return ESP_FAIL;
  }
  ESP_RETURN_ON_ERROR(
    nvs_get_str(nvsHandle, WIFI_SSID_NVS_NAME, settings->wifiSSID, &(settings->wifiSSIDLen)),
    TAG, "failed to retrieve wifi ssid from nvs after malloc"
  );
  /* retrieve wifi password */
  ESP_RETURN_ON_ERROR(
    nvs_get_str(nvsHandle, WIFI_PASS_NVS_NAME, NULL, &(settings->wifiPassLen)),
    TAG, "failed to retrieve wifi ssid from nvs"
  );
  if ((settings->wifiPass = malloc(settings->wifiPassLen)) != NULL) {
    free(settings->wifiSSID);
    return ESP_FAIL;
  }
  ESP_RETURN_ON_ERROR(
    nvs_get_str(nvsHandle, WIFI_PASS_NVS_NAME, settings->wifiPass, &(settings->wifiPassLen)),
    TAG, "failed to retrieve wifi ssid from nvs after malloc"
  );
  /* retrieve api key */
  ESP_RETURN_ON_ERROR(
    nvs_get_str(nvsHandle, API_KEY_NVS_NAME, NULL, &(settings->apiKeyLen)),
    TAG, "failed to retrieve wifi ssid from nvs"
  );
  if ((settings->apiKey = malloc(settings->apiKeyLen)) != NULL) {
    free(settings->wifiSSID);
    free(settings->wifiPass);
    return ESP_FAIL;
  }
  ESP_RETURN_ON_ERROR(
    nvs_get_str(nvsHandle, API_KEY_NVS_NAME, settings->apiKey, &(settings->apiKeyLen)),
    TAG, "failed to retrieve wifi ssid from nvs after malloc"
  );
  return ESP_OK;
}

/**
 * Initializes the dot matrix ICs by issuing commands to
 * the I2C gatekeeper.
 */
esp_err_t initDotMatrices(QueueHandle_t I2CQueue) {
    ESP_LOGI(TAG, "initializing dot matrices");
    ESP_RETURN_ON_ERROR(
      dotsReset(I2CQueue),
      TAG, "failed to reset dot matrices"
    );
    ESP_RETURN_ON_ERROR(
      dotsSetGlobalCurrentControl(I2CQueue, DOTS_GLOBAL_CURRENT),
      TAG, "failed to change dot matrices global current control"
    );
    for (int i = 1; i <= NUM_LEDS; i++) {
      ESP_RETURN_ON_ERROR(
        dotsSetScaling(I2CQueue, i, 0xFF, 0xFF, 0xFF),
        TAG, "failed to set dot scaling"
      );
    }
    ESP_RETURN_ON_ERROR(
      dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION),
      TAG, "failed to set dot matrices into normal operation mode"
    );
    return ESP_OK;
}

/**
 * Creates the I2C gatekeeper task, which handles commands
 * issued to it via the I2CQueue. The gatekeeper is intended
 * to be the only task that interacts with the I2C peripheral
 * in order to keep dot matrices in known states.
 */
esp_err_t createI2CGatekeeperTask(QueueHandle_t I2CQueue) {
  BaseType_t success;
  I2CGatekeeperTaskParams *params;
  /* input guards */
  ESP_RETURN_ON_FALSE(
    (I2CQueue != NULL), ESP_FAIL,
    TAG, "cannot create I2C gatekeeper task with NULL I2C command queue"
  );
  /* create parameters */
  params = malloc(sizeof(I2CGatekeeperTaskParams));
  ESP_RETURN_ON_FALSE(
    (params != NULL), ESP_FAIL,
    TAG, "failed to allocate memory"
  );
  params->I2CQueue = I2CQueue;
  params->port = I2C_PORT;
  params->sdaPin = SDA_PIN;
  params->sclPin = SCL_PIN;
  /* create task */
  success = xTaskCreate(vI2CGatekeeperTask, "I2CGatekeeper", I2C_GATEKEEPER_STACK,
                        params, I2C_GATEKEEPER_PRIO, NULL);
  ESP_RETURN_ON_FALSE(
    (success == pdPASS), ESP_FAIL,
    TAG, "failed to create I2C gatekeeper"
  );
  return ESP_OK;
}

/**
 * Creates a dot worker task, which handles commands issued
 * to the pool of dot workers via the dotQueue. The task
 * performs TomTom api requests and sends I2C commands to
 * update the color of LEDs based on the request results.
 */
esp_err_t createDotWorkerTask(char *name, QueueHandle_t dotQueue, QueueHandle_t I2CQueue, char *apiKey) {
  BaseType_t success;
  struct dotWorkerTaskParams *params;
  /* input guards */
  ESP_RETURN_ON_FALSE(
    (dotQueue != NULL), ESP_FAIL,
    TAG, "cannot create dot worker task with NULL dot command queue"
  );
  ESP_RETURN_ON_FALSE(
    (I2CQueue != NULL), ESP_FAIL,
    TAG, "cannot create dot worker task with NULL I2C command queue"
  );
  ESP_RETURN_ON_FALSE(
    (apiKey != NULL), ESP_FAIL,
    TAG, "cannot create dot worker task with NULL api key"
  );
  if (name == NULL) {
    name = "worker";
  }
  /* allocate parameters */
  params = malloc(sizeof(struct dotWorkerTaskParams)); // task parameters are given via reference, not copy
  ESP_RETURN_ON_FALSE(
    (params != NULL), ESP_FAIL,
    TAG, "failed to allocate memory for dot worker task parameters"
  );
  params->dotQueue = dotQueue;
  params->I2CQueue = I2CQueue;
  params->apiKey = apiKey;
  /* create task */
  success = xTaskCreate(vDotWorkerTask, name, DOTS_WORKER_STACK, 
                        params, DOTS_WORKER_PRIO, NULL);
  if (success != pdPASS) {
    ESP_LOGE(TAG, "failed to create dot worker task");
    free(params);
    return ESP_FAIL;
  }
  return ESP_OK;
}

/**
 * Initialized the direction LEDs by configuring
 * GPIO pins.
 */
esp_err_t initDirectionLEDs(void) {
    /* Set GPIO directions */
    ESP_RETURN_ON_ERROR(
      gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT),
      TAG, "failed to set north led pin to output"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT),
      TAG, "failed to set east led pin to output"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT),
      TAG, "failed to set south led pin to output"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT),
      TAG, "failed to set west led pin to output"
    );
    /* Set GPIO levels */
    ESP_RETURN_ON_ERROR(
      gpio_set_level(LED_NORTH_PIN, 0),
      TAG, "failed to turn off direction led"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_level(LED_EAST_PIN, 0),
      TAG, "failed to turn off direction led"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_level(LED_SOUTH_PIN, 0),
      TAG, "failed to turn off direction led"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_level(LED_WEST_PIN, 0),
      TAG, "failed to turn off direction led"
    );
    return ESP_OK;
}

/**
 * Initializes the direction button by configuring its
 * GPIO pin and attaching dirButtonISR() on a negative edge.
 */
esp_err_t initDirectionButton(bool *toggle) {
  struct dirButtonISRParams *params = malloc(sizeof(params));
  params->mainTask = xTaskGetCurrentTaskHandle();
  params->toggle = toggle;
  ESP_RETURN_ON_ERROR(
    gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT), // pin has an external pullup
    TAG, "failed to set gpio direction of direction button"
  );
  ESP_RETURN_ON_ERROR(
    gpio_set_intr_type(T_SW_PIN, GPIO_INTR_NEGEDGE),
    TAG, "failed to set direction button interrupt type"
  );
  ESP_RETURN_ON_ERROR(
    gpio_isr_handler_add(T_SW_PIN, dirButtonISR, params),
    TAG, "failed to setup direction button ISR"
  );
  ESP_RETURN_ON_ERROR(
    gpio_intr_enable(T_SW_PIN),
    TAG, "failed to enable interrupts for direction button"
  );
  return ESP_OK;
}

/**
 * Enables the direction button interrupt,
 * which is handled by dirButtonISR().
 */
esp_err_t enableDirectionButtonIntr(void) {
  ESP_RETURN_ON_ERROR(
    gpio_intr_enable(T_SW_PIN),
    TAG, "failed to enable interrupts for direction button"
  );
  return ESP_OK;
}

/**
 * Disables the direction button interrupt,
 * which is handled by dirButtonISR().
 */
esp_err_t disableDirectionButtonIntr(void) {
  ESP_RETURN_ON_ERROR(
    gpio_intr_disable(T_SW_PIN),
    TAG, "failed to disable interrupts for direction button"
  );
  return ESP_OK;
}

/**
 * Turns off all LEDs by directly issuing commands to the
 * I2C gatekeeper. This may interfere with commands issued
 * by dot workers if they do not finish their work quickly,
 * so it is recommended to wait for both the dot command queue
 * and I2C command queue to be empty before calling this function.
 */
esp_err_t clearLEDs(QueueHandle_t I2CQueue, Direction dir) {
  switch (dir) {
    case NORTH:
      for(int i = NUM_LEDS; i > 0; i--) {
        ESP_RETURN_ON_ERROR(
          dotsSetColor(I2CQueue, i, 0x00, 0x00, 0x00),
          TAG, "failed to clear led"
        );
      }
      break;
    case SOUTH:
      for(int i = 1; i <= NUM_LEDS; i++) {
        ESP_RETURN_ON_ERROR(
          dotsSetColor(I2CQueue, i, 0x00, 0x00, 0x00),
          TAG, "failed to clear led"
        );
      }
      break;
  }

  
  return ESP_OK;
}

/**
 * Issues a command to the dot queue, which will be handled
 * by one of the dot workers, to update all LEDs based on the
 * latest TomTom data for the provided direction.
 */
esp_err_t updateLEDs(QueueHandle_t dotQueue, Direction dir) {
  esp_err_t ret = ESP_OK;
  DotCommand dot;
  int startNdx, endNdx, northLvl, southLvl, eastLvl, westLvl;
  /* input guards */
  ESP_RETURN_ON_FALSE(
    (dotQueue != NULL), ESP_FAIL,
    TAG, "updateLEDs received NULL dot queue handle"
  );
  /* update direction LEDs */
  switch (dir) {
    case NORTH:
      startNdx = NUM_LEDS;
      endNdx = 1;
      northLvl = 1;
      eastLvl = 0;
      southLvl = 0;
      westLvl = 1;
      break;
    case SOUTH:
      startNdx = 1;
      endNdx = NUM_LEDS;
      northLvl = 0;
      eastLvl = 1;
      southLvl = 1;
      westLvl = 0;
      break;
    default:
      ESP_LOGE(TAG, "updateLEDs received unknown direction");
      goto error_with_dir_leds_off;
      break;
  }
  ESP_GOTO_ON_ERROR(
    gpio_set_level(LED_NORTH_PIN, northLvl), error_with_dir_leds_off,
    TAG, "failed to turn change north led"
  );
  ESP_GOTO_ON_ERROR(
    gpio_set_level(LED_EAST_PIN, eastLvl), error_with_dir_leds_off,
    TAG, "failed to turn change east led"
  );
  ESP_GOTO_ON_ERROR(
    gpio_set_level(LED_SOUTH_PIN, southLvl), error_with_dir_leds_off,
    TAG, "failed to turn change south led"
  );
  ESP_GOTO_ON_ERROR(
    gpio_set_level(LED_WEST_PIN, westLvl), error_with_dir_leds_off,
    TAG, "failed to turn change west led"
  );
  /* send commands to update all dot LEDs */
  dot.dir = dir;
  if (startNdx <= endNdx) {
    for (int i = startNdx; i <= endNdx; i++) {
      dot.ledNum = i;
      while (pdPASS != xQueueSendToBack(dotQueue, &dot, INT_MAX)) {
        ESP_LOGI(TAG, "failed to add dot to queue, retrying...");
      }
    }
  } else {
    for (int i = startNdx; i >= endNdx; i--) {
      dot.ledNum = i;
      while (pdPASS != xQueueSendToBack(dotQueue, &dot, INT_MAX)) {
        ESP_LOGI(TAG, "failed to add dot to queue, retrying...");
      }
    }
  }
  /* wait for all LEDs to be updated */
  while (uxQueueMessagesWaiting(dotQueue) > 0) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  return ret;
error_with_dir_leds_off:
  /* wait for all LEDs to be updated */
  while (uxQueueMessagesWaiting(dotQueue) > 0) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  ESP_ERROR_CHECK(gpio_set_level(LED_NORTH_PIN, 0));
  ESP_ERROR_CHECK(gpio_set_level(LED_EAST_PIN, 0));
  ESP_ERROR_CHECK(gpio_set_level(LED_SOUTH_PIN, 0));
  ESP_ERROR_CHECK(gpio_set_level(LED_WEST_PIN, 0));
  return ret;
}

/**
 * An error handling function for errors which are
 * not due to a user settings issue. Traps the task
 * in a delay forever loop after setting the error
 * LED high.
 */
void spinForever(void) {
  /* turn on error led */
  ESP_LOGE(TAG, "Spinning forever due to an unhandleable error!");
  gpio_set_direction(ERR_LED_PIN, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level(ERR_LED_PIN, 1);
  for (;;) {
    vTaskDelay(INT_MAX);
  }
}

/**
 * An error handling function for errors which are
 * due to a user settings issue. Sets the error LED
 * high and queries the user for new settings, then
 * turns the LED off and restarts the application.
 */
void updateSettingsAndRestart(nvs_handle_t nvsHandle) {
  /* Errors are assumed to be settings issues, thus let the
  user update settings, then restart the system */
  ESP_LOGE(TAG, "Requesting settings update due to a handleable error");
  /* turn on error led to indicate a settings update is requested */
  gpio_set_direction(ERR_LED_PIN, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level(ERR_LED_PIN, 1);
  /* request settings update from user */
  if (getNvsEntriesFromUser(nvsHandle) != ESP_OK) {
    spinForever();
  }
  /* turn off error led and restart */
  gpio_set_level(ERR_LED_PIN, 0);
  esp_restart();
}