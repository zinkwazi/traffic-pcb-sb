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
#include "initialize.h"
#include "main_types.h"
#include "refresh.h"
#include "routines.h"
#include "utilities.h"

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
static esp_err_t mainRefresh(MainTaskState *state, MainTaskResources *res, LEDData typicalNorth[static MAX_NUM_LEDS_REG + 1], LEDData typicalSouth[static MAX_NUM_LEDS_REG + 1]) {
  esp_err_t err;
  LEDData currentSpeeds[MAX_NUM_LEDS_REG + 1];
  /* input guards */
  if (state == NULL ||
      res == NULL ||
      res->client == NULL ||
      typicalNorth == NULL ||
      typicalSouth == NULL)
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
  /* retrieve updated data */
  err = refreshData(currentSpeeds, res->client, state->dir, LIVE, res->errRes);
  if (err != ESP_OK) return err;
  /* refresh LEDs */
  err = indicateDirection(state->dir);
  if (err != ESP_OK) return err;
  switch (state->dir)
  {
    case NORTH:
      err = refreshBoard(currentSpeeds, typicalNorth, CURVED_LINE_REVERSE);
      break;
    case SOUTH:
      err = refreshBoard(currentSpeeds, typicalSouth, CURVED_LINE);
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
    notificationValue = ulTaskNotifyTake(pdTRUE, INT_MAX);
  } while (notificationValue != 1); // a timeout occurred while waiting for button press
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
    LEDData typicalNorthSpeeds[MAX_NUM_LEDS_REG + 1];
    LEDData typicalSouthSpeeds[MAX_NUM_LEDS_REG + 1];
    /* set task priority */
    vTaskPrioritySet(NULL, CONFIG_MAIN_PRIO);
    /* print firmware information */
    ESP_LOGE(TAG, "Traffic Firmware " VERBOSE_VERSION_STR);
    ESP_LOGE(TAG, "OTA binary: " FIRMWARE_UPGRADE_URL);
    /* initialize application */
    err = initializeMatrices();
    FATAL_IF_ERR(err, res.errRes);
    /* quick clear LEDs, maybe leftover from reboot */
    (void) quickClearBoard(); // let this fail, refresh will occur after
    err = initializeIndicatorLEDs();
    FATAL_IF_ERR(err, res.errRes);
    err = initializeApplication(&state, &res);
    FATAL_IF_ERR(err, res.errRes);
    
    /* retrieve speeds */
    err = refreshData(typicalNorthSpeeds, res.client, NORTH, TYPICAL, res.errRes);
    FATAL_IF_ERR(err, res.errRes);
    err = refreshData(typicalSouthSpeeds, res.client, SOUTH, TYPICAL, res.errRes);
    FATAL_IF_ERR(err, res.errRes);
    /* handle requests to update all LEDs */
    err = enableDirectionButtonIntr();
    FATAL_IF_ERR(err, res.errRes);
    while (true) {
      err = mainRefresh(&state, &res, typicalNorthSpeeds, typicalSouthSpeeds);
      (void) mainWaitForTaskNotification(&res); // extremely noticeable error, allows clearing after notification recieved
      if (err == REFRESH_ABORT)
      {
        err = quickClearBoard();
        FATAL_IF_ERR(err, res.errRes);
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