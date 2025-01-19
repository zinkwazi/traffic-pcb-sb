/**
 * routines.c
 */

#include "routines.h"

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

/* Component includes */
#include "dots_commands.h"
#include "led_registers.h"

void dirButtonISR(void *params) {
  TaskHandle_t mainTask = ((DirButtonISRParams *) params)->mainTask;
  TickType_t *lastISR = ((DirButtonISRParams *) params)->lastISR;
  bool *toggle = ((DirButtonISRParams *) params)->toggle;

  /* debounce interrupt */
  TickType_t currentTick = xTaskGetTickCountFromISR();
  if (currentTick - *lastISR < pdMS_TO_TICKS(CONFIG_DEBOUNCE_PERIOD)) {
    return;
  } else {
    *lastISR = currentTick;
  }

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  *toggle = true;
  vTaskNotifyGiveFromISR(mainTask, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void otaButtonISR(void *params) {
  TaskHandle_t otaTask = (TaskHandle_t) params;

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(otaTask, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

esp_timer_handle_t createRefreshTimer(TaskHandle_t mainTask, bool *toggle) {
  static RefreshTimerParams params;
  esp_timer_create_args_t timerArgs;
  esp_timer_handle_t ret;
  /* input guards */
  if (mainTask == NULL || toggle == NULL) {
    return false;
  }
  /* copy parameters */
  params.mainTask = mainTask;
  params.toggle = toggle;
  /* create timer */
  timerArgs.callback = refreshTimerCallback;
  timerArgs.arg = &params;
  timerArgs.dispatch_method = ESP_TIMER_ISR;
  timerArgs.name = "refreshTimer";
  if (esp_timer_create(&timerArgs, &ret) != ESP_OK) {
    return NULL;
  }
  return ret;
}

void refreshTimerCallback(void *params) {
  TaskHandle_t mainTask = (TaskHandle_t) params;

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(mainTask, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void timerFlashDirCallback(void *params) {
  int *currentOutput = (int *) params;
  *currentOutput = (*currentOutput == 1) ? 0 : 1;
  gpio_set_level(LED_NORTH_PIN, *currentOutput);
  gpio_set_level(LED_EAST_PIN, *currentOutput);
  gpio_set_level(LED_WEST_PIN, *currentOutput);
  gpio_set_level(LED_SOUTH_PIN, *currentOutput);
}