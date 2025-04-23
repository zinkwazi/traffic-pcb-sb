/**
 * actions.c
 * 
 * Contains functions that scheduling jobs and interact with SNTP.
 */
#include "actions.h"
#include "actions_pi.h" // contains static function declarations

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/adc_types.h"

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
/* The time between sampling the photoresistor pin voltage during averaging */
#define UPDATE_BRIGHTNESS_WAIT_MS 5
/* the maximum ADC value expected from the photoresistor, which is absolute darkness*/
#define MAX_BRIGHTNESS_LEVEL (4000)

/* The times of day to check whether an OTA update is available */
#define CHECK_OTA_AVAILABLE_TIMES_SIZE (sizeof(checkOTAAvailableTimes) / sizeof(time_t))
static const time_t checkOTAAvailableTimes[] = {
    HOURS_TO_SECS(0) + MINS_TO_SECS(0), // midnight
    HOURS_TO_SECS(11) + MINS_TO_SECS(0), // 11:00am
    HOURS_TO_SECS(17) + MINS_TO_SECS(0), // 5:00pm
};

int64_t getUpdateTrafficDataPeriodSec(void)
{
    return UPDATE_TRAFFIC_DATA_PERIOD_SEC;
}

int64_t getUpdateBrightnessPeriodSec(void)
{
    return UPDATE_BRIGHTNESS_PERIOD_SEC;
}

const time_t *getCheckOTAAvailableTimes(void)
{
    return checkOTAAvailableTimes;
}

size_t getCheckOTAAvailableTimesSize(void)
{
    return CHECK_OTA_AVAILABLE_TIMES_SIZE;
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
        case ACTION_UPDATE_BRIGHTNESS:
            ESP_LOGI(TAG, "Performing Action: ACTION_UPDATE_BRIGHTNESS");
            return handleActionUpdateBrightness();
        case ACTION_QUERY_OTA:
            ESP_LOGI(TAG, "Performing Action: ACTION_QUERY_OTA");
            return handleActionQueryOTA();
        default:
            return ESP_ERR_NOT_FOUND;
    }
    return ESP_ERR_NOT_FOUND;
}

STATIC_IF_NOT_TEST esp_err_t handleActionUpdateData(void)
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

STATIC_IF_NOT_TEST esp_err_t handleActionUpdateBrightness(void)
{
    esp_err_t err;
    const int readingsLen = 5;
    int readings[readingsLen]; // reading multiple times for averaging
    readings[0] = 0;
    readings[1] = 0;
    readings[2] = 0;
    readings[3] = 0;
    readings[4] = 0;
    int64_t ambientLevel;
    uint8_t currentValue;

    /* read ambient brightness level */
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = ADC_UNIT_1, // contains GPIO4
        .clk_src = 0, // default clock source
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    err = adc_oneshot_new_unit(&adc_cfg, &adc_handle);
    if (err != ESP_OK) THROW_ERR(err);

    adc_oneshot_chan_cfg_t adc_chan_cfg = {
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    err = adc_oneshot_config_channel(adc_handle, PHOTO_ADC_CHAN, &adc_chan_cfg);
    if (err != ESP_OK)
    {
        (void) adc_oneshot_del_unit(adc_handle);
        THROW_ERR(err);
    }

    for (int i = 0; i < readingsLen; i++)
    {
        err = adc_oneshot_read(adc_handle, PHOTO_ADC_CHAN, &(readings[i]));
        if (err != ESP_OK)
        {
            (void) adc_oneshot_del_unit(adc_handle);
            THROW_ERR(err);
        }
        vTaskDelay(pdMS_TO_TICKS(UPDATE_BRIGHTNESS_WAIT_MS));
    }

    ambientLevel = (readings[0] + readings[1] + readings[2] + readings[3] + readings[4]) / readingsLen;
    ESP_LOGI(TAG, "ambient brightness level: %lld", ambientLevel);

    /* clamp ambient level */
    ambientLevel = (ambientLevel > MAX_BRIGHTNESS_LEVEL) ? MAX_BRIGHTNESS_LEVEL : ambientLevel;

    ESP_LOGI(TAG, "brightness percent: %f", ((double) (MAX_BRIGHTNESS_LEVEL - ambientLevel) / MAX_BRIGHTNESS_LEVEL));
    currentValue = (uint8_t) (CONFIG_GLOBAL_LED_CURRENT * ((double) (MAX_BRIGHTNESS_LEVEL - ambientLevel) / MAX_BRIGHTNESS_LEVEL));

    /* clamp min current value */
    currentValue = (currentValue < 0x10) ? 0x10 : currentValue;
    err = matSetGlobalCurrentControl(currentValue);

    err = adc_oneshot_del_unit(adc_handle);
    if (err != ESP_OK) THROW_ERR(err);
    return ESP_OK;
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
    if (patchUpdate && updateAvailable)
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

    } else if (updateAvailable)
    {
        (void) indicateOTAUpdate(); // best effort
    }
    #else
    #error "Unsupported hardware version!"
    #endif

    return ESP_OK;
}



