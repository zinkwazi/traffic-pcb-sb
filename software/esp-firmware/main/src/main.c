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
#include "input_queue.h"
#include "main_types.h"
#include "refresh.h"
#include "routines.h"
#include "utilities.h"

#include "initialize.h"

#define TAG "app_main"

#define NOT_QUICK     (false)

static Direction currDir = SOUTH; // the currently displayed traffic direction

static esp_err_t mainRefresh(bool toggleDirection);

/**
 * @brief The entrypoint task of the application.
 * 
 * Initializes other tasks, requests user settings, then handles button presses
 * for LED refreshes. As this task handles user input, it should not do a lot
 * of processing. Rather, heavy processing is primarily left to vDotWorkerTask.
 */
void app_main(void)
{
    MainCommand cmd;
    bool toggleDirection = false;
    bool prevCmdAborted = false;
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

    /* initialize application */
    err = initializeApplication();
    if (err != ESP_OK) throwFatalError(); // if app_errors component uninitialized, this still traps
    err = enableHoldDirButton();
    if (err != ESP_OK) throwFatalError();
    err = enableQuickDirButton();
    if (err != ESP_OK) throwFatalError();
    err = enableOTAButton();
    if (err != ESP_OK) throwFatalError();

    /* TODO: check nighttime mode status */

    /* initial refresh */
    cmd = MAIN_CMD_QUICK_DIR_BTN;
    (void) quickClearBoard(currDir); // TODO: handle I2C errors
    err = mainRefresh(toggleDirection);
    if (REFRESH_ABORT == err) {
      quickClearBoard(currDir);
      prevCmdAborted = true;
    }
    
    /* handle commands from various sources */
    while (true) {
      /* wait for next command */
      if (pdFALSE == xQueueReceive(inputQueue, &cmd, pdMS_TO_TICKS(CONFIG_LED_REFRESH_PERIOD * 60 * 1000)))
      {
        /* no received input, timeout occurred */
        cmd = MAIN_CMD_TIMEOUT;
      }

      /* handle current command */
      toggleDirection = false;
      switch (cmd)
      {
        case MAIN_CMD_QUICK_DIR_BTN:
          ESP_LOGI(TAG, "quick");
          toggleDirection = true;
          /* falls through */
        case MAIN_CMD_TIMEOUT:
          ESP_LOGI(TAG, "refresh");
          err = decrementAbortCount(); // keep refresh abort counter synchronized with queue items
          if (ESP_OK != err)
          {
            ESP_LOGE(TAG, "could not decrement abort counter");
            throwFatalError();
          }
          if (!abortCountZero()) continue;
          if (!prevCmdAborted) {
            err = clearBoard(currDir, NOT_QUICK);
            if (REFRESH_ABORT == err) {
              quickClearBoard(currDir);
              prevCmdAborted = true;
              continue;
            }
          }
          err = mainRefresh(toggleDirection);
          if (REFRESH_ABORT == err) {
            quickClearBoard(currDir);
            prevCmdAborted = true;
            continue;
          }
          if (ESP_OK != err) throwFatalError();
          break;
        case MAIN_CMD_HOLD_DIR_BTN:
          ESP_LOGI(TAG, "long");
          /* TODO: toggle nighttime mode enabled */
          break;
        case MAIN_CMD_SCHEDULED:
          ESP_LOGI(TAG, "scheduled");
          /* TODO: nighttime mode activation/deactivation */
          break;
        default:
          ESP_LOGE(TAG, "received unknown main command: %d", cmd);
          throwFatalError(); // should not be possible
      }

      prevCmdAborted = false;
    }

    /* emergency handling */
    ESP_LOGE(TAG, "Main task is exiting!");
    throwFatalError();
}

/**
 * @brief Handles everything that needs to happen after a toggle button press.
 * 
 * @param[in] toggleDirection Whether to toggle the current direction or not.
 * @param[in] typicalNoth An array filled with typical north road segment speeds.
 * @param[in] typicalSouth An array filled with typical south road segment speeds.
 * 
 * @returns ESP_OK if successful.
 * REFRESH_ABORT if a new command is available on the main queue.
 */
static esp_err_t mainRefresh(bool toggleDirection) {
  esp_err_t err;

  /* handle toggle button press */
  if (toggleDirection && NORTH == currDir) {
    currDir = SOUTH;
  } else if (toggleDirection && SOUTH == currDir) {
    currDir = NORTH;
  }

  /* refresh LEDs */
  err = indicateDirection(currDir);
  if (err != ESP_OK) return err;
  switch (currDir)
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