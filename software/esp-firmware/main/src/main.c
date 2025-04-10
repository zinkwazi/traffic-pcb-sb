/**
 * main.c
 * 
 * This file contains the entrypoint task
 * for the application (app_main) and
 * configuration including function hooks.
 */

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "animations.h"
#include "api_connect.h"
#include "app_errors.h"
#include "led_registers.h"

#include "indicators.h"
#include "main_types.h"
#include "refresh.h"
#include "routines.h"
#include "utilities.h"

#include "initialize.h"

#define TAG "app_main"

/**
 * @brief Handles everything that needs to happen after a toggle button press.
 * 
 * @param[in] state A pointer to the main task state.
 * @param[in] res A pointer to the main task's resources.
 * @param[in] typicalNoth An array filled with typical north road segment speeds.
 * @param[in] typicalSouth An array filled with typical south road segment speeds.
 * 
 * @returns ESP_OK if successful.
 */
static esp_err_t mainRefresh(MainTaskState *state, MainTaskResources *res, LEDData typicalNorth[static MAX_NUM_LEDS_REG], LEDData typicalSouth[static MAX_NUM_LEDS_REG]) {
  esp_err_t err;
  /* input guards */
  if (state == NULL ||
      res == NULL)
  {
    return ESP_ERR_INVALID_ARG;
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
  /* refresh LEDs */
  err = indicateDirection(state->dir);
  if (err != ESP_OK) return err;
  switch (state->dir)
  {
    case NORTH:
      err = refreshBoard(NORTH, CURVED_LINE_NORTH);
      break;
    case SOUTH:
      err = refreshBoard(SOUTH, CURVED_LINE_SOUTH);
      break;
    default:
      err = ESP_FAIL;
      break;
  }
  return err;
}

/**
 * @brief Waits for a user button press or timeout, which is indicated
 *        via a task notification.
 * 
 * @param res A pointer to the main task's resources
 * 
 * @returns ESP_OK if successful.
 */
static esp_err_t mainWaitForTaskNotification(MainTaskResources *res) {
  esp_err_t err;
  /* input guards */
  if (res == NULL ||
      res->refreshTimer == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }
  /* setup timeout timer */
  err = esp_timer_restart(res->refreshTimer, ((uint64_t) CONFIG_LED_REFRESH_PERIOD) * 60 * 1000000); // restart timer if toggle is pressed
  if (err == ESP_ERR_INVALID_STATE) { // meaning: timer has not yet started
    err = esp_timer_start_periodic(res->refreshTimer, ((uint64_t) CONFIG_LED_REFRESH_PERIOD) * 60 * 1000000);
  }
  if (err != ESP_OK) return err;
  /* wait for button press or timer reset */
  uint32_t notificationValue;
  do {
    notificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  } while (notificationValue == 0); // a timeout occurred while waiting for button press
  err = esp_timer_stop(res->refreshTimer);
  return err;
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
    LEDData typicalNorthSpeeds[MAX_NUM_LEDS_REG];
    LEDData typicalSouthSpeeds[MAX_NUM_LEDS_REG];
    /* set task priority */
    vTaskPrioritySet(NULL, CONFIG_MAIN_PRIO);
    /* print firmware information */
    err = initializeLogChannel();
    FATAL_IF_ERR(err, res.errRes);
    ESP_LOGE(TAG, "Hardware Version: " HARDWARE_VERSION_STR);
    ESP_LOGE(TAG, "Firmware Version: " FIRMWARE_VERSION_STR);
    ESP_LOGE(TAG, "OTA binary: " FIRMWARE_UPGRADE_URL);
    /* initialize application */
    err = initializeMatrices();
    FATAL_IF_ERR(err, res.errRes);
    /* quick clear LEDs, maybe leftover from reboot */
    (void) quickClearBoard(SOUTH); // let this fail, refresh will occur after
    err = initializeIndicatorLEDs();
    FATAL_IF_ERR(err, res.errRes);
    err = initializeApplication(&state, &res);
    FATAL_IF_ERR(err, res.errRes);
    
    /* handle requests to update all LEDs */
    err = enableDirectionButtonIntr();
    FATAL_IF_ERR(err, res.errRes);
    while (true) {
      err = mainRefresh(&state, &res, typicalNorthSpeeds, typicalSouthSpeeds); // never consumes task notifications, only checks them
      (void) mainWaitForTaskNotification(&res); // extremely noticeable error, allows clearing after notification recieved
      if (err == REFRESH_ABORT)
      {
        err = quickClearBoard(state.dir);
        FATAL_IF_ERR(err, res.errRes);
      } else if (err == REFRESH_ABORT_NO_CLEAR)
      {
        /* do nothing, improves efficiency slightly */
      } else {
        err = clearBoard(state.dir, false);
        if (err == REFRESH_ABORT)
        {
          err = quickClearBoard(state.dir);
          FATAL_IF_ERR(err, res.errRes);
          // consume this task notification
          (void) ulTaskNotifyTake(pdTRUE, 0);
        }
        FATAL_IF_ERR(err, res.errRes);
      }
    }
    /* This task has nothing left to do, but should not exit */
    ESP_LOGE(TAG, "Main task is exiting!");
    throwFatalError(res.errRes, false);
    for (;;) {
      vTaskDelay(INT_MAX);
    }
}