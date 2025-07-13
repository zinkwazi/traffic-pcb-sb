/**
 * actions.c
 * 
 * Contains functions that schedule jobs and interact with SNTP.
 */
#include "actions.h"
#include "actions_pi.h" // contains static function declarations

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "wrap_esp_http_client.h"

#include "led_matrix.h"
#include "utilities.h"
#include "main_types.h"
#include "ota.h"
#include "indicators.h"
#include "led_registers.h"
#include "refresh.h"
#include "traffic_data.h"
#include "pinout.h"

#define TAG "actions"

#define HOURS_TO_SECS(h) (h * 60 * 60)
#define MINS_TO_SECS(m) (m * 60)

/* The number of seconds between updating traffic data from the server */
#define UPDATE_TRAFFIC_DATA_PERIOD_SEC MINS_TO_SECS(20)

/* The number of seconds between checking the ambient brightness level */
#define UPDATE_BRIGHTNESS_PERIOD_SEC 2

static const time_t otaSchedule[] = {
    HOURS_TO_SECS(0) + MINS_TO_SECS(0), // midnight
    HOURS_TO_SECS(11) + MINS_TO_SECS(0), // 11:00am
    HOURS_TO_SECS(17) + MINS_TO_SECS(0), // 5:00pm
};

static const time_t startNighttimeModeSchedule[] = {
    HOURS_TO_SECS(21) + MINS_TO_SECS(0), // 9:00pm
};

static const time_t endNighttimeModeSchedule[] = {
    HOURS_TO_SECS(5) + MINS_TO_SECS(0), // 5:00am
};

/* The list of actions and when they are scheduled for every day.
   WARNING: actions are scheduled one at a time, so if two actions
   are scheduled very close together, then one of them may be missed.
   This architecture can be changed, but it is unlikely to be necessary. */
static const ScheduledAction scheduledActions[] = {
    { // checkOTAAvailable
        .action = ACTION_QUERY_OTA,
        .schedule = otaSchedule,
        .scheduleLen = sizeof(otaSchedule) / sizeof(time_t),
    },
    { // startNighttimeMode
        .action = ACTION_START_NIGHTTIME_MODE,
        .schedule = startNighttimeModeSchedule,
        .scheduleLen = sizeof(startNighttimeModeSchedule) / sizeof(time_t),
    },
    { // endNighttimeMode
        .action = ACTION_END_NIGHTTIME_MODE,
        .schedule = endNighttimeModeSchedule,
        .scheduleLen = sizeof(endNighttimeModeSchedule) / sizeof(time_t),
    },
};

int64_t getUpdateTrafficDataPeriodSec(void)
{
    return UPDATE_TRAFFIC_DATA_PERIOD_SEC;
}

int64_t getUpdateBrightnessPeriodSec(void)
{
    return UPDATE_BRIGHTNESS_PERIOD_SEC;
}

const ScheduledAction *getScheduledActions(void)
{
    return scheduledActions;
}

size_t getScheduledActionsLen(void)
{
    return sizeof(scheduledActions) / sizeof(ScheduledAction);
}

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
#if CONFIG_HARDWARE_VERSION == 2
        case ACTION_UPDATE_BRIGHTNESS:
            ESP_LOGI(TAG, "Performing Action: ACTION_UPDATE_BRIGHTNESS");
            return handleActionUpdateBrightness();
        case ACTION_QUERY_OTA:
            ESP_LOGI(TAG, "Performing Action: ACTION_QUERY_OTA");
            return handleActionQueryOTA();
        case ACTION_START_NIGHTTIME_MODE:
            ESP_LOGI(TAG, "Performing Action: ACTION_START_NIGHTTIME_MODE");
            return handleActionStartNighttimeMode();
        case ACTION_END_NIGHTTIME_MODE:
            ESP_LOGI(TAG, "Performing Action: ACTION_END_NIGHTTIME_MODE");
            return handleActionEndNighttimeMode();
#endif /* CONFIG_HARDWARE_VERSION */
        default:
            return ESP_ERR_NOT_FOUND;
    }
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Updates road segment data by querying all data files from the server.
 * This happens here because it is a low priority action.
 * 
 * @returns ESP_OK if successful.
 */
STATIC_IF_NOT_TEST esp_err_t handleActionUpdateData(void)
{
    esp_err_t err;
    esp_http_client_handle_t client;
    LEDData northData[MAX_NUM_LEDS_REG];
    LEDData southData[MAX_NUM_LEDS_REG];

    /* query typical data from server, falling back to nvs if necessary */
    client = initHttpClient();
    if (client == NULL) THROW_ERR(ESP_FAIL);

    err = refreshData(northData, client, NORTH, LIVE);
    if (err != ESP_OK) goto handle_error;
    err = refreshData(southData, client, SOUTH, LIVE);
    if (err != ESP_OK) goto handle_error;

    /* update static data */
    err = borrowTrafficData(LIVE, portMAX_DELAY);
    if (err != ESP_OK) goto handle_error;
    err = updateTrafficData(northData, MAX_NUM_LEDS_REG, NORTH, LIVE);
    if (err != ESP_OK) goto handle_error;
    err = updateTrafficData(southData, MAX_NUM_LEDS_REG, SOUTH, LIVE);
    if (err != ESP_OK) goto handle_error;
    err = releaseTrafficData(LIVE);
    if (err != ESP_OK) goto handle_error;
    
    err = ESP_HTTP_CLIENT_CLEANUP(client);
    if (err != ESP_OK) throwFatalError();
    return ESP_OK;
handle_error:
    err = ESP_HTTP_CLIENT_CLEANUP(client);
    if (err != ESP_OK) throwFatalError();
    return ESP_FAIL;
}

#if CONFIG_HARDWARE_VERSION == 1
    /* feature unsupported */
#elif CONFIG_HARDWARE_VERSION == 2

/**
 * @brief Updates global LED brightness based on the current ambient light
 * level as determined by the photoresistor.
 * 
 * @requires:
 * - led_matrix component initialized.
 * 
 * @returns ESP_OK if successful.
 */
STATIC_IF_NOT_TEST esp_err_t handleActionUpdateBrightness(void)
{
    return matSetGCCByAmbientLight();
}


/**
 * @brief Queries the firmware version file to check whether an OTA update is
 * available or not. If so, indicateOTAUpdate is called.
 * 
 * @note If a patch update is available, but not a major/minor version change,
 * then this function sends a task notification to the OTA task which initiates
 * a firmware upgrade.
 * 
 * @requires:
 * - OTA task initialized.
 */
STATIC_IF_NOT_TEST esp_err_t handleActionQueryOTA(void)
{
    /* query most recent server firmware version and indicate if an update is available */
    #if CONFIG_HARDWARE_VERSION == 1
    /* feature unsupported */
    #elif CONFIG_HARDWARE_VERSION == 2
    bool updateAvailable = false;
    bool patchUpdate = false;

    (void) queryOTAUpdateAvailable(&updateAvailable, &patchUpdate); // allow firmware updates even if this
                                                                    // function fails in order to fix 
                                                                    // potential issues in this function
    if (updateAvailable) // all updates are mandatory
    {
        TaskHandle_t otaTask = getOTATask();
        if (otaTask == NULL)
        {
            (void) indicateOTAUpdate();
            return ESP_OK;
        }
        BaseType_t success = xTaskNotify(otaTask, 0xFF, eSetBits);
        if (success != pdPASS)
        {
            (void) indicateOTAUpdate();
        }
    }
    #else
    #error "Unsupported hardware version!"
    #endif

    return ESP_OK;
}

/**
 * @brief Starts nighttime mode, which locks refreshes and turns off LEDs.
 */
STATIC_IF_NOT_TEST esp_err_t handleActionStartNighttimeMode(void)
{
    /* turn off refreshes */
    lockBoardRefresh();

    /* tell main to refresh. There is a case where the board is already
    refreshing where this causes a quick clear */


    return ESP_OK;
}

STATIC_IF_NOT_TEST esp_err_t handleActionEndNighttimeMode(void)
{
    /* turn on refreshes */
    unlockBoardRefresh();

    /* tell main to refresh. There is a case where the board is
    already refreshing where this causes a quick clear */
    
    
    return ESP_OK;
}

#else
#error "Unsupported hardware version!"
#endif /* CONFIG_HARDWARE_VERSION */

