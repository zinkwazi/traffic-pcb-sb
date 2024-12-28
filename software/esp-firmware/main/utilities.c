/**
 * utilities.c
 * 
 * This file contains functions that may be useful to tasks
 * contained in various other header files.
 */

#include "utilities.h"

/* IDF component includes */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
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
#include "worker.h"
#include "utilities.h"
#include "wifi.h"

/* Component includes */
#include "api_config.h"
#include "tomtom.h"
#include "led_locations.h"
#include "dots_commands.h"
#include "led_registers.h"

/**
 * 
 */
void dirButtonISR(void *params) {
  TaskHandle_t mainTask = ((struct dirButtonISRParams *) params)->mainTask;
  bool *toggle = ((struct dirButtonISRParams *) params)->toggle;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  *toggle = true;
  vTaskNotifyGiveFromISR(mainTask, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void otaButtonISR(void *params) {
  TaskHandle_t otaTask = (TaskHandle_t) params;
  xTaskNotifyGive(otaTask);
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
 * Enabled when a settings update is requested, this callback
 * toggles all the direction LEDs.
 */
void timerFlashDirCallback(void *params) {
  int *currentOutput = (int *) params;
  *currentOutput = (*currentOutput == 1) ? 0 : 1;
  gpio_set_level(LED_NORTH_PIN, *currentOutput);
  gpio_set_level(LED_EAST_PIN, *currentOutput);
  gpio_set_level(LED_WEST_PIN, *currentOutput);
  gpio_set_level(LED_SOUTH_PIN, *currentOutput);
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
  const unsigned short bufLen = 256;
  char c;
  char buf[bufLen];
  ESP_LOGI(TAG, "Querying settings from user...");
  printf("\n\nWifi SSID: ");
  fflush(stdout);
  for (int i = 0; i < bufLen; i++) {
    buf[i] = getchar();
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
    buf[i] = getchar();
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
    buf[i] = getchar();
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
  if (nvs_get_str(nvsHandle, WIFI_SSID_NVS_NAME, NULL, &(settings->wifiSSIDLen)) != ESP_OK) {
    return ESP_FAIL;
  }
  if ((settings->wifiSSID = malloc(settings->wifiSSIDLen)) == NULL) {
    return ESP_FAIL;
  }
  if (nvs_get_str(nvsHandle, WIFI_SSID_NVS_NAME, settings->wifiSSID, &(settings->wifiSSIDLen)) != ESP_OK) {
    return ESP_FAIL;
  }
  /* retrieve wifi password */
  if (nvs_get_str(nvsHandle, WIFI_PASS_NVS_NAME, NULL, &(settings->wifiPassLen)) != ESP_OK) {
    return ESP_FAIL;
  }
  if ((settings->wifiPass = malloc(settings->wifiPassLen)) == NULL) {
    free(settings->wifiSSID);
    return ESP_FAIL;
  }
  if (nvs_get_str(nvsHandle, WIFI_PASS_NVS_NAME, settings->wifiPass, &(settings->wifiPassLen)) != ESP_OK) {
    return ESP_FAIL;
  }
  /* retrieve api key */
  if (nvs_get_str(nvsHandle, API_KEY_NVS_NAME, NULL, &(settings->apiKeyLen)) != ESP_OK) {
    return ESP_FAIL;
  }
  if ((settings->apiKey = malloc(settings->apiKeyLen)) == NULL) {
    free(settings->wifiSSID);
    free(settings->wifiPass);
    return ESP_FAIL;
  }
  if (nvs_get_str(nvsHandle, API_KEY_NVS_NAME, settings->apiKey, &(settings->apiKeyLen)) != ESP_OK) {
    return ESP_FAIL;
  }
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
  if (I2CQueue == NULL) {
    return ESP_FAIL;
  }
  /* create parameters */
  params = malloc(sizeof(I2CGatekeeperTaskParams));
  if (params == NULL) {
    return ESP_FAIL;
  }
  params->I2CQueue = I2CQueue;
  params->port = I2C_PORT;
  params->sdaPin = SDA_PIN;
  params->sclPin = SCL_PIN;
  /* create task */
  success = xTaskCreate(vI2CGatekeeperTask, "I2CGatekeeper", I2C_GATEKEEPER_STACK,
                        params, I2C_GATEKEEPER_PRIO, NULL);
  return (success == pdPASS) ? ESP_OK : ESP_FAIL;
}

/**
 * Creates a dot worker task, which handles commands issued
 * to the pool of dot workers via the dotQueue. The task
 * performs TomTom api requests and sends I2C commands to
 * update the color of LEDs based on the request results.
 * 
 * workerNum is an important parameter that corresponds to
 * the worker event group bit which the task will use to
 * indicate that it is idle. These indicators are used by
 * the main task to synchronize clearing LEDs. workerNum
 * should be between 0 and 7.
 */
esp_err_t createDotWorkerTask(char *name, int workerNum, QueueHandle_t dotQueue, QueueHandle_t I2CQueue, EventGroupHandle_t workerEvents, char *apiKey, bool *errorOccurred, SemaphoreHandle_t errorOccurredMutex) {
  BaseType_t success;
  struct dotWorkerTaskParams *params;
  /* input guards */
  if (dotQueue == NULL || I2CQueue == NULL || apiKey == NULL || errorOccurred == NULL || errorOccurredMutex == NULL) {
    return ESP_FAIL;
  }
  if (name == NULL) {
    name = "worker";
  }
  if (workerNum < 0 || workerNum > 7) {
    return ESP_FAIL;
  }
  /* allocate parameters */
  params = malloc(sizeof(struct dotWorkerTaskParams)); // task parameters are given via reference, not copy
  ESP_RETURN_ON_FALSE(
    (params != NULL), ESP_FAIL,
    TAG, "failed to allocate memory for dot worker task parameters"
  );
  if (params == NULL) {
    return ESP_FAIL;
  }
  params->dotQueue = dotQueue;
  params->I2CQueue = I2CQueue;
  params->workerEvents = workerEvents;
  params->workerIdleBit = (1 << workerNum);
  params->apiKey = apiKey;
  params->errorOccurred = errorOccurred;
  params->errorOccurredMutex = errorOccurredMutex;
  if (params->errorOccurredMutex == NULL) {
    free(params);
    return ESP_FAIL;
  }
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
    if (gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT) != ESP_OK) {
      return ESP_FAIL;
    }
    if (gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT) != ESP_OK) {
      return ESP_FAIL;
    }
    if (gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT) != ESP_OK) {
      return ESP_FAIL;
    }
    if (gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT) != ESP_OK) {
      return ESP_FAIL;
    }
    /* Set GPIO levels */
    if (gpio_set_level(LED_NORTH_PIN, 0) != ESP_OK) {
      return ESP_FAIL;
    }
    if (gpio_set_level(LED_EAST_PIN, 0) != ESP_OK) {
      return ESP_FAIL;
    }
    if (gpio_set_level(LED_SOUTH_PIN, 0) != ESP_OK) {
      return ESP_FAIL;
    }
    if (gpio_set_level(LED_WEST_PIN, 0) != ESP_OK) {
      return ESP_FAIL;
    }
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
  if (gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT) != ESP_OK) { // pin has an external pullup
    return ESP_FAIL;
  }
  if (gpio_set_intr_type(T_SW_PIN, GPIO_INTR_NEGEDGE) != ESP_OK) {
    return ESP_FAIL;
  }
  if (gpio_isr_handler_add(T_SW_PIN, dirButtonISR, params) != ESP_OK) {
    return ESP_FAIL;
  }
  if (gpio_intr_enable(T_SW_PIN) != ESP_OK) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t initIOButton(TaskHandle_t otaTask) {
  if (gpio_set_pull_mode(IO_SW_PIN, GPIO_PULLUP_ONLY) != ESP_OK) {
    return ESP_FAIL;
  }
  if (gpio_pullup_en(IO_SW_PIN) != ESP_OK) {
    return ESP_FAIL;
  }
  if (gpio_set_direction(IO_SW_PIN, GPIO_MODE_INPUT) != ESP_OK) {
    return ESP_FAIL;
  }
  if (gpio_set_intr_type(IO_SW_PIN, GPIO_INTR_NEGEDGE) != ESP_OK) {
    return ESP_FAIL;
  }
  if (gpio_isr_handler_add(IO_SW_PIN, otaButtonISR, otaTask) != ESP_OK) {
    return ESP_FAIL;
  }
  if (gpio_intr_enable(IO_SW_PIN) != ESP_OK) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

/**
 * Enables the direction button interrupt,
 * which is handled by dirButtonISR().
 */
esp_err_t enableDirectionButtonIntr(void) {
  return gpio_intr_enable(T_SW_PIN);
}

/**
 * Disables the direction button interrupt,
 * which is handled by dirButtonISR().
 */
esp_err_t disableDirectionButtonIntr(void) {
  return gpio_intr_disable(T_SW_PIN);
}

esp_err_t quickClearLEDs(QueueHandle_t dotQueue) {
  DotCommand command;
  /* empty the queue */
  while (xQueueReceive(dotQueue, &command, 0) == pdTRUE) {}
  /* send quick clear command */
  command.type = QUICK_CLEAR;
  if (xQueueSend(dotQueue, &command, 0) != pdTRUE) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

/**
 * Turns off all LEDs by directly issuing commands to the
 * I2C gatekeeper. This may interfere with commands issued
 * by dot workers if they do not finish their work quickly,
 * so it is recommended to wait for both the dot command queue
 * and I2C command queue to be empty before calling this function.
 */
esp_err_t clearLEDs(QueueHandle_t dotQueue, Direction currDir) {
  DotCommand command;
  /* empty the queue */
  while (xQueueReceive(dotQueue, &command, 0) == pdTRUE) {}
  /* determine clearing direction */
  switch (currDir) {
    case NORTH:
      command.type = CLEAR_NORTH;
      break;
    case SOUTH:
      command.type = CLEAR_SOUTH;
      break;
    default:
      break;
  }
  /* send clear command */
  if (xQueueSend(dotQueue, &command, 0) != pdTRUE) {
    return ESP_FAIL;
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
  DotCommand command;
  int northLvl, southLvl, eastLvl, westLvl;
  /* input guards */
  if (dotQueue == NULL) {
    return ESP_FAIL;
  }
  /* update direction LEDs */
  switch (dir) {
    case NORTH:
      command.type = REFRESH_NORTH;
      northLvl = 1;
      eastLvl = 0;
      southLvl = 0;
      westLvl = 1;
      break;
    case SOUTH:
      command.type = REFRESH_SOUTH;
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
  /* send commands to update all dot LEDs */
  while (pdPASS != xQueueSendToBack(dotQueue, &command, INT_MAX)) {
    ESP_LOGW(TAG, "failed to add dot to queue, retrying...");
  }
  return ret;
error_with_dir_leds_off:
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
void spinForever(bool *errorOccurred, SemaphoreHandle_t errorOccurredMutex) {
  /* turn on error led */
  ESP_LOGE(TAG, "Spinning forever due to an unhandleable error!");
  if (errorOccurred == NULL || errorOccurredMutex == NULL || !boolWithTestSet(errorOccurred, errorOccurredMutex)) {
    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(ERR_LED_PIN, 1);
  }
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
void updateSettingsAndRestart(nvs_handle_t nvsHandle, bool *errorOccurred, SemaphoreHandle_t errorOccurredMutex) {
  /* Errors are assumed to be settings issues, thus let the
  user update settings, then restart the system */
  ESP_LOGE(TAG, "Requesting settings update due to a handleable error");
  /* turn on error led and flash direction leds to indicate a settings update is requested */
  if (errorOccurred == NULL || errorOccurredMutex == NULL || !boolWithTestSet(errorOccurred, errorOccurredMutex)) {
    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(ERR_LED_PIN, 1);
  }
  int currentFlashOutput = 0;
  esp_timer_create_args_t flashTimerArgs = {
    .callback = timerFlashDirCallback,
    .dispatch_method = ESP_TIMER_ISR,
    .name = "flashDirTimer",
    .arg = &currentFlashOutput,
  };
  esp_timer_handle_t flashDirTimer;
  if (esp_timer_create(&flashTimerArgs, &flashDirTimer) != ESP_OK) {
    spinForever(NULL, NULL);
  }
  if (esp_timer_start_periodic(flashDirTimer, CONFIG_ERROR_PERIOD * 1000) != ESP_OK) {
    spinForever(NULL, NULL);
  }
  /* request settings update from user */
  if (getNvsEntriesFromUser(nvsHandle) != ESP_OK) {
    spinForever(NULL, NULL);
  }
  /* turn off error led and direction leds and restart */
  gpio_set_level(ERR_LED_PIN, 0);
  gpio_set_level(LED_NORTH_PIN, 0);
  gpio_set_level(LED_EAST_PIN, 0);
  gpio_set_level(LED_SOUTH_PIN, 0);
  gpio_set_level(LED_WEST_PIN, 0);
  esp_restart();
}

/**
 * Atomically tests and sets val to true. Returns whether
 * val was already true.
 */
bool boolWithTestSet(bool *val, SemaphoreHandle_t mutex) {
    if (*val) {
        return true;
    }
    while (xSemaphoreTake(mutex, INT_MAX) != pdTRUE) {}
    if (*val) {
        xSemaphoreGive(mutex);
        return true;
    }
    *val = true;
    xSemaphoreGive(mutex);
    return false;
}