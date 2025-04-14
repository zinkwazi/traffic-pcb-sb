/**
 * actions.c
 * 
 * Contains functions that scheduling jobs and interact with SNTP.
 */
#include "actions.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "main_types.h"
#include "ota.h"
#include "indicators.h"
#include "led_registers.h"
#include "refresh.h"
#include "traffic_data.h"

#define TAG "actions"

#define HOURS_TO_SECS(h) (h * 60 * 60)
#define MINS_TO_SECS(m) (m * 60)

/* The number of seconds between updating traffic data from the server */
#define UPDATE_TRAFFIC_DATA_PERIOD_SEC MINS_TO_SECS(20)
#define CHECK_OTA_AVAILABLE_TIMES_SIZE (sizeof(checkOTAAvailableTimes) / sizeof(time_t))

/* The times of day to check whether an OTA update is available */
static const time_t checkOTAAvailableTimes[] = {
    HOURS_TO_SECS(0) + MINS_TO_SECS(0), // midnight
    HOURS_TO_SECS(11) + MINS_TO_SECS(0), // 11:00am
    HOURS_TO_SECS(15) + MINS_TO_SECS(20), // 3:20pm
    HOURS_TO_SECS(17) + MINS_TO_SECS(0), // 5:00pm
    HOURS_TO_SECS(23) + MINS_TO_SECS(0),
};

int64_t getUpdateTrafficDataPeriodSec(void)
{
    return UPDATE_TRAFFIC_DATA_PERIOD_SEC;
}

const time_t *getCheckOTAAvailableTimes(void)
{
    return checkOTAAvailableTimes;
}

size_t getCheckOTAAvailableTimesSize(void)
{
    return CHECK_OTA_AVAILABLE_TIMES_SIZE;
}

static esp_err_t handleActionUpdateData(void);
static esp_err_t handleActionQueryOTA(void);

/**
 * @brief Performs the given action.
 * 
 * @param[in] action The action to perform.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_NOT_FOUND if the action handler was not registered in the function.
 * ESP_FAIL if the action failed.
 */
esp_err_t handleAction(Action action)
{
    switch (action)
    {
        case ACTION_UPDATE_DATA:
            ESP_LOGI(TAG, "Performing Action: ACTION_UPDATE_DATA");
            return handleActionUpdateData();
        case ACTION_QUERY_OTA:
            ESP_LOGI(TAG, "Performing Action: ACTION_QUERY_OTA");
            return handleActionQueryOTA();
        default:
            return ESP_ERR_NOT_FOUND;
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t handleActionUpdateData()
{
    esp_err_t err;
    esp_http_client_handle_t client;
    LEDData northData[MAX_NUM_LEDS_REG];
    LEDData southData[MAX_NUM_LEDS_REG];

    /* query typical data from server, falling back to nvs if necessary */
    client = initHttpClient();
    err = refreshData(northData, client, NORTH, LIVE);
    if (err != ESP_OK)
    {
        err = esp_http_client_cleanup(client);
        if (err != ESP_OK) throwFatalError();
        return ESP_FAIL;
    }
    err = refreshData(southData, client, SOUTH, LIVE);
    if (err != ESP_OK)
    {
        err = esp_http_client_cleanup(client);
        if (err != ESP_OK) throwFatalError();
        return ESP_FAIL;
    }

    /* update static data */
    err = borrowTrafficData(LIVE, portMAX_DELAY);
    if (err != ESP_OK) return ESP_FAIL;
    err = updateTrafficData(northData, MAX_NUM_LEDS_REG, NORTH, LIVE);
    if (err != ESP_OK) return ESP_FAIL;
    err = updateTrafficData(southData, MAX_NUM_LEDS_REG, SOUTH, LIVE);
    if (err != ESP_OK) return ESP_FAIL;
    err = releaseTrafficData(LIVE);
    if (err != ESP_OK) return ESP_FAIL;
    
    return ESP_OK;
}

static esp_err_t handleActionQueryOTA()
{
    /* query most recent server firmware version and indicate if an update is available */
    #if CONFIG_HARDWARE_VERSION == 1
    /* feature unsupported */
    #elif CONFIG_HARDWARE_VERSION == 2
    bool updateAvailable;

    (void) queryOTAUpdateAvailable(&updateAvailable); // allow firmware updates even if this
                                                // function fails in order to fix 
                                                // potential issues in this function
    if (updateAvailable)
    {
    indicateOTAAvailable();
    }
    #else
    #error "Unsupported hardware version!"
    #endif

    return ESP_OK;
}



