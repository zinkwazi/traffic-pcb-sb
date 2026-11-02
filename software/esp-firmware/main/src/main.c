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
#include "led_matrix.h"

#include "refresh_task.h"

#include "initialize.h"

#define TAG "app_main"

/* priority of the refresh task; below main and OTA */
#define REFRESH_PRIO        (2)
/* the traffic direction displayed at startup */
#define STARTUP_DIR         (SOUTH)
#define STARTUP_ANIM        (CURVED_LINE_SOUTH)

static void mainTask(void);

/**
 * @brief The entrypoint task of the application.
 * 
 * Creates the refresh task, initializes the application (which retrieves
 * typical and live traffic data from the server, falling back to
 * non-volatile storage), provides typical data for both directions to
 * the refresh task, then displays live traffic data.
 */
void app_main(void)
{
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

    /* create the refresh task first: it resets the LED matrices, which must
    happen before indicator LEDs are set, and the action task may send it
    night mode commands as soon as it is created */
    err = createRefreshTask(NULL, REFRESH_PRIO);
    if (err != ESP_OK) throwFatalError();

    /* connect to wifi and retrieve traffic data, among other things */
    err = initializeApplication();
    if (err != ESP_OK) throwFatalError();

    /* the refresh task waits for typical data in both directions before
    displaying any live data */
    err = refreshTypical(NORTH, NULL, portMAX_DELAY);
    if (err != ESP_OK) throwFatalError();
    err = refreshTypical(SOUTH, NULL, portMAX_DELAY);
    if (err != ESP_OK) throwFatalError();

    /* run main task */
    mainTask();

    /* block rather than spin so lower priority tasks can run */
    while (true)
    {
        vTaskDelay(portMAX_DELAY);
    }

    /* emergency handling */
    ESP_LOGE(TAG, "Main task is exiting!");
    throwFatalError();
}

/**
 * Retrieves input from the main input queue and handles them.
 */
static void mainTask(void)
{
    esp_err_t err;
    bool currDirection = STARTUP_DIR;
    volatile bool prevRefreshDataReceived = true;
    volatile bool prevRefreshFrameCleared = false;
    volatile bool prevToggleNightModeReceived = true;
    bool nightModeActive = false;
    MainCommand command = MAIN_CMD_ERROR;

    err = refreshDisableNightMode(&prevToggleNightModeReceived, portMAX_DELAY);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disable night mode: err = 0x%x", err);
    }

    err = refreshBoard(STARTUP_DIR, STARTUP_ANIM, &prevRefreshDataReceived, NULL, portMAX_DELAY);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initially refresh board: err = 0x%x", err);
    }

    while (true)
    {
        xQueueReceive(mainInputQueue, &command, portMAX_DELAY);

        switch (command)
        {
            case MAIN_CMD_SCHEDULED:
                if (!prevRefreshDataReceived) break;
                switch (currDirection)
                {
                    case NORTH:
                        err = refreshBoard(NORTH, CURVED_LINE_NORTH, &prevRefreshDataReceived, &prevRefreshFrameCleared, 0);
                        break;
                    case SOUTH:
                        err = refreshBoard(SOUTH, CURVED_LINE_SOUTH, &prevRefreshDataReceived, &prevRefreshFrameCleared, 0);
                        break;
                }
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to execute scheduled refresh: err = 0x%x", err);
                }
                break;
            case MAIN_CMD_QUICK_DIR_BTN:
                if (!prevRefreshDataReceived) break;
                if (!prevRefreshFrameCleared)
                {
                    /* keep the same direction, ie. invert later toggle */
                    currDirection = (currDirection == NORTH) ? SOUTH : NORTH;
                }

                switch (currDirection)
                {
                    case NORTH:
                        currDirection = SOUTH;
                        err = refreshBoard(SOUTH, CURVED_LINE_SOUTH, &prevRefreshDataReceived, &prevRefreshFrameCleared, 0);
                        break;
                    case SOUTH:
                        currDirection = NORTH;
                        err = refreshBoard(NORTH, CURVED_LINE_NORTH, &prevRefreshDataReceived, &prevRefreshFrameCleared, 0);
                        break;
                }
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to execute button refresh: err = 0x%x", err);
                }
                break;
            case MAIN_CMD_HOLD_DIR_BTN:
                if (nightModeActive)
                {
                    err = refreshDisableNightMode(&prevToggleNightModeReceived, portMAX_DELAY);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to disable night mode: err = 0x%x", err);
                    } else
                    {
                        nightModeActive = false;
                    }
                } else
                {
                    err = refreshEnableNightMode(&prevToggleNightModeReceived, portMAX_DELAY);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to enable night mode: err = 0x%x", err);
                    } else
                    {
                        nightModeActive = true;
                    }
                }
                break;
            case MAIN_CMD_TIMEOUT:
                break;
            case MAIN_CMD_ERROR:
                ESP_LOGW(TAG, "Main FSM received MAIN_CMD_ERROR");
                break;
        }
    }
}
