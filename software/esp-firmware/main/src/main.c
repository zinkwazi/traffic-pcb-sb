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
#include "led_matrix.h"

#include "refresh_task.h"

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

    err = createRefreshTask(NULL);
    if (err != ESP_OK) throwFatalError();

    LEDData data[MAX_NUM_LEDS_REG];
    for (uint32_t i = 0; i < MAX_NUM_LEDS_REG; i++)
    {
        data[i].ledNum = i + 1;
        data[i].speed = i;
    }

    err = refreshLEDs(data, SOUTH, portMAX_DELAY);
    if (err != ESP_OK) throwFatalError();

    while (true) {}

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