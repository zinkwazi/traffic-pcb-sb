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
#include "tasks.h"
#include "utilities.h"
#include "wifi.h"
#include "routines.h"

/* Component includes */
#include "dots_commands.h"
#include "led_registers.h"

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
  return ret;
}

esp_err_t removeExtraMainNvsEntries(nvs_handle_t nvsHandle) {
  esp_err_t ret;
  nvs_iterator_t nvs_iter;
  if (nvs_entry_find_in_handle(nvsHandle, NVS_TYPE_ANY, &nvs_iter) != ESP_OK) {
    return ESP_FAIL;
  }
  ret = nvs_entry_next(&nvs_iter);
  while (ret != ESP_OK) {
    nvs_entry_info_t info;
    if (nvs_entry_info(nvs_iter, &info) != ESP_OK) {
      return ESP_FAIL;
    }
    if (strcmp(info.namespace_name, "main") == 0 &&
            (strcmp(info.key, WIFI_SSID_NVS_NAME) == 0 ||
             strcmp(info.key, WIFI_PASS_NVS_NAME) == 0))
    {
      ret = nvs_entry_next(&nvs_iter);
      continue;
    }
    ESP_LOGI(TAG, "removing nvs entry: %s", info.key);
    if (nvs_erase_key(nvsHandle, info.key) != ESP_OK) {
      return ESP_FAIL;
    }
    ret = nvs_entry_next(&nvs_iter);
  }
  if (nvs_commit(nvsHandle) != ESP_OK) {
    return ESP_FAIL;
  }
  if (ret == ESP_ERR_INVALID_ARG) {
    return ESP_FAIL;
  }
  return ESP_OK;
}


esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle) {
  const unsigned short bufLen = 256;
  char c;
  char buf[bufLen];
  ESP_LOGI(TAG, "Querying settings from user...");
  printf("\nWifi SSID: ");
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
  fflush(stdout);
  ESP_RETURN_ON_ERROR(
    nvs_set_str(nvsHandle, WIFI_SSID_NVS_NAME, buf),
    TAG, "failed to write wifi SSID to non-volatile storage"
  );
  printf("\nWifi Password: ");
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
  fflush(stdout);
  ESP_RETURN_ON_ERROR(
    nvs_set_str(nvsHandle, WIFI_PASS_NVS_NAME, buf),
    TAG, "failed to write wifi password to non-volatile storage"
  );
  ESP_RETURN_ON_ERROR(
    nvs_commit(nvsHandle),
    TAG, "failed to commit NVS changes"
  );
  return ESP_OK;
}

esp_err_t retrieveNvsEntries(nvs_handle_t nvsHandle, struct UserSettings *settings)
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
  return ESP_OK;
}

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

esp_err_t initDirectionButton(TickType_t *lastISR, bool *toggle) {
  struct dirButtonISRParams *params = malloc(sizeof(params));
  params->mainTask = xTaskGetCurrentTaskHandle();
  params->lastISR = lastISR;
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

esp_err_t enableDirectionButtonIntr(void) {
  return gpio_intr_enable(T_SW_PIN);
}

esp_err_t disableDirectionButtonIntr(void) {
  return gpio_intr_disable(T_SW_PIN);
}

esp_err_t quickClearLEDs(QueueHandle_t dotQueue) {
  WorkerCommand command;
  /* empty the queue */
  while (xQueueReceive(dotQueue, &command, 0) == pdTRUE) {}
  /* send quick clear command */
  command.type = QUICK_CLEAR;
  if (xQueueSend(dotQueue, &command, 0) != pdTRUE) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t clearLEDs(QueueHandle_t dotQueue, Direction currDir) {
  WorkerCommand command;
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

esp_err_t updateLEDs(QueueHandle_t dotQueue, Direction dir) {
  esp_err_t ret = ESP_OK;
  WorkerCommand command;
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

void throwNoConnError(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }
  
  switch (errRes->err) {
    case NO_ERR:
      errRes->err = NO_SERVER_CONNECT_ERR;
      /* falls through */
    case NO_SERVER_CONNECT_ERR:
      if (errRes->errTimer == NULL) {
        startErrorFlashing(errRes, true);
      }
      break;
    case HANDLEABLE_ERR:
      errRes->err = HANDLEABLE_AND_NO_SERVER_CONNECT_ERR;
      // do not flash because solid takes priority
      break;
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      break;
    case FATAL_ERR:
      throwFatalError(errRes, true);
      break;
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

void throwHandleableError(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  if (errRes->errTimer != NULL) {
    stopErrorFlashing(errRes, true);
  }
  INDICATE_ERR();
  switch (errRes->err) {
    case NO_ERR:
      errRes->err = HANDLEABLE_ERR;
      break;
    case NO_SERVER_CONNECT_ERR:
      errRes->err = HANDLEABLE_AND_NO_SERVER_CONNECT_ERR;
      break;
    case HANDLEABLE_ERR:
      // cannot have multiple handleable errors at once
      /* falls through */
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      // cannot have multiple handleable errors at once
      ESP_LOGE(TAG, "multiple HANDLEABLE_ERR thrown!");
      /* falls through */
    case FATAL_ERR:
      throwFatalError(errRes, true);
      break;
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

void throwFatalError(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  ESP_LOGE(TAG, "FATAL_ERR thrown!");
  if (errRes->errTimer != NULL) {
    stopErrorFlashing(errRes, true);
  }
  gpio_set_level(ERR_LED_PIN, 1);
  errRes->err = FATAL_ERR;
  
  xSemaphoreGive(errRes->errMutex); // give up mutex in caller's name
  for (;;) {
    vTaskDelay(INT_MAX);
  }
}

void resolveNoConnError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  ESP_LOGW(TAG, "resolving NO_SERVER_CONNECT_ERR");
  if (errRes->errTimer != NULL) {
    stopErrorFlashing(errRes, true);
    gpio_set_level(ERR_LED_PIN, 0);
  }
  switch (errRes->err) {
    case NO_SERVER_CONNECT_ERR:
      errRes->err = NO_ERR;
      break;
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      errRes->err = HANDLEABLE_ERR;
      break;
    case NO_ERR:
      /* falls through */
    case HANDLEABLE_ERR:
      if (resolveNone) {
        break;
      }
      ESP_LOGE(TAG, "resolving NO_SERVER_CONNECT_ERR without its error state");
      /* falls through */
    case FATAL_ERR:
      /* falls through */
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

void resolveHandleableError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  ESP_LOGW(TAG, "resolving HANDLEABLE_ERR");
  switch (errRes->err) {
    case HANDLEABLE_ERR:
      errRes->err = NO_ERR;
      gpio_set_level(ERR_LED_PIN, 0);
      break;
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      errRes->err = NO_SERVER_CONNECT_ERR;
      if (errRes->errTimer == NULL) {
        startErrorFlashing(errRes, true);
      }
      break;
    case NO_ERR:
      /* falls through */
    case NO_SERVER_CONNECT_ERR:
      if (resolveNone) {
        break;
      }
      ESP_LOGE(TAG, "resolving HANDLEABLE_ERR without its error state");
      /* falls through */
    case FATAL_ERR:
      /* falls through */
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

void startErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex) {
    const esp_timer_create_args_t timerArgs = {
      .callback = timerFlashErrCallback,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "errorTimer",
    };

    if (!callerHasErrMutex) {
      while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
    }

    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
    if (esp_timer_create(&timerArgs, &(errRes->errTimer)) != ESP_OK) {
      throwFatalError(errRes, true);
    }
    if (esp_timer_start_periodic(errRes->errTimer, CONFIG_ERROR_PERIOD * 1000) != ESP_OK) {
      esp_timer_delete(errRes->errTimer);
      errRes->errTimer = NULL;
      throwFatalError(errRes, true);
    }

    if (!callerHasErrMutex) {
      xSemaphoreGive(errRes->errMutex);
    }
}

void stopErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  esp_timer_stop(errRes->errTimer);
  esp_timer_delete(errRes->errTimer);
  errRes->errTimer = NULL;

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

void updateNvsSettings(nvs_handle_t nvsHandle, ErrorResources *errRes) {
  throwHandleableError(errRes, false); // turns on error LED
  
  /* flash direction LEDs to indicate settings update is requested */
  const esp_timer_create_args_t flashTimerArgs = {
    .callback = timerFlashDirCallback,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "flashDirTimer",
    .arg = NULL,
  };
  esp_timer_handle_t flashDirTimer;
  if (esp_timer_create(&flashTimerArgs, &flashDirTimer) != ESP_OK) {
    throwFatalError(errRes, false);
  }
  if (esp_timer_start_periodic(flashDirTimer, CONFIG_ERROR_PERIOD * 1000) != ESP_OK) {
    throwFatalError(errRes, false);
  }
  /* request settings update from user */
  if (getNvsEntriesFromUser(nvsHandle) != ESP_OK) {
    throwFatalError(errRes, false);
  }

  resolveHandleableError(errRes, false, false); // returns error LED to previous state
}