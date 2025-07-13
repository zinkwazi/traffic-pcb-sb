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

#include "esp_mac.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "animations.h"
#include "api_connect.h"
#include "app_err.h"
#include "app_errors.h"
#include "led_registers.h"
#include "indicators.h"
#include "input.h"
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
static esp_err_t mainRefresh(MainTaskState *state, MainTaskResources *res) {
  esp_err_t err;
  /* input guards */
  if (state == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
  if (res == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);

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
      THROW_ERR(ESP_FAIL);
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
    uint8_t mac[6];
    /* set task priority */
    vTaskPrioritySet(NULL, CONFIG_MAIN_PRIO);
    /* print firmware information */
    err = initializeLogChannel();
    if (err != ESP_OK) throwFatalError();
    ESP_LOGE(TAG, "Hardware Version: " HARDWARE_VERSION_STR);
    ESP_LOGE(TAG, "Firmware Version: " FIRMWARE_VERSION_STR);
    ESP_LOGE(TAG, "OTA Binary: " FIRMWARE_UPGRADE_URL);
    err = esp_base_mac_addr_get(mac);
    if (err != ESP_OK) throwFatalError();
    ESP_LOGE(TAG, "Base MAC Address: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    err = initializeApplication(&state, &res);
    if (err != ESP_OK) throwFatalError(); // if app_errors component uninitialized, this still traps

    /* TODO: check whether nighttime mode should be enabled or not */

    
    /* handle requests to update all LEDs */
    err = enableHoldDirButton();
    if (err != ESP_OK) throwFatalError();
    err = enableQuickDirButton();
    if (err != ESP_OK) throwFatalError();
    err = enableOTAButton();
    if (err != ESP_OK) throwFatalError();
    while (true) {
      err = mainRefresh(&state, &res); // never consumes task notifications, only checks them
      (void) mainWaitForTaskNotification(&res); // extremely noticeable error, allows clearing after notification recieved
      if (err == REFRESH_ABORT)
      {
        err = quickClearBoard(state.dir);
        if (err != ESP_OK && err != REFRESH_ABORT) throwFatalError();
      } else if (err == REFRESH_ABORT_NO_CLEAR)
      {
        /* do nothing, improves efficiency slightly */
      } else if (err == ESP_OK)
      {
        err = clearBoard(state.dir, false);
        if (err == REFRESH_ABORT)
        {
          err = quickClearBoard(state.dir);
          if (err != ESP_OK) throwFatalError();
          // consume this task notification bc it indicates quick clear board
          (void) ulTaskNotifyTake(pdTRUE, 0);
        }
        if (err != ESP_OK) throwFatalError();
      } else {
        /* mainRefresh returned bad error code */
        throwFatalError();
      }
    }
    /* This task has nothing left to do, but should not exit */
    ESP_LOGE(TAG, "Main task is exiting!");
    throwFatalError();
}