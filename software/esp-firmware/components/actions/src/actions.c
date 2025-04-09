/**
 * actions.c
 * 
 * Contains functions that scheduling jobs and interact with SNTP.
 */
#include "actions.h"

#include <time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_errors.h"
#include "ota.h"
#include "indicators.h"

#define TAG "actions"

#define HOURS_TO_SECS(h) (h * 60 * 60)
#define MINS_TO_SECS(m) (m * 60)

/* The SNTP sync interval, in minutes */
#define SYNC_INTERVAL (120)

/* The number of seconds between updating traffic data from the server */
#define UPDATE_TRAFFIC_DATA_PERIOD_SEC MINS_TO_SECS(1)

#define SECONDS_IN_DAY (HOURS_TO_SECS(24))

#define CHECK_OTA_AVAILABLE_TIMES_SIZE (sizeof(checkOTAAvailableTimes) / sizeof(time_t))

/* The times of day to check whether an OTA update is available */
static const time_t checkOTAAvailableTimes[] = {
    HOURS_TO_SECS(0) + MINS_TO_SECS(0), // midnight
    HOURS_TO_SECS(11) + MINS_TO_SECS(0), // 11:00am
    HOURS_TO_SECS(17) + MINS_TO_SECS(0), // 5:00pm
};

static int64_t secsUntilNextJob(void);
static void jobTimerCallback(void *arg);
static void updateDataTimerCallback(void *arg);

static const esp_timer_create_args_t updateTrafficTimerCfg = {
    .callback = updateDataTimerCallback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "dataTimer",
};

static esp_timer_handle_t updateTrafficTimer = NULL;

static const esp_timer_create_args_t nextJobTimerCfg = {
    .callback = jobTimerCallback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "jobTimer",
};

static esp_timer_handle_t nextJobTimer = NULL;

/**
 * @brief Initializes SNTP and timers to run jobs at particular times of day
 *        and starts both the data update and job timer.
 * 
 * @requires:
 *  - esp_timer library initialized by esp_timer_init.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t initJobs(void)
{
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t err;
    int64_t nextJobSecs;

    // Set timezone to Los Angeles time zone (PST/PDT)
    setenv("TZ", "PST8PDT,M3.2.0/2,M11.1.0/2", 1);
    tzset();

    /* initialize and sync SNTP */
    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setoperatingmode(SNTP_SYNC_MODE_IMMED); // override default
    err = esp_netif_sntp_init(&sntp_cfg);
    if (err != ESP_OK) return err;
    sntp_set_sync_interval(pdMS_TO_TICKS(SYNC_INTERVAL * 1000 * 60));
    do {
        ESP_LOGI(TAG, "waiting for SNTP sync...");
        err = esp_netif_sntp_sync_wait(INT_MAX);
    } while (err != ESP_OK);
    ESP_LOGI(TAG, "SNTP sync complete...");
    err = esp_netif_sntp_start();
    if (err != ESP_OK) return err;

    /* initialize timers */
    err = esp_timer_create(&updateTrafficTimerCfg, &updateTrafficTimer);
    if (err != ESP_OK) return err;
    err = esp_timer_create(&nextJobTimerCfg, &nextJobTimer);
    if (err != ESP_OK) return err;

    /* start timers */
    ESP_LOGI(TAG, "traffic timer set for %d seconds from now", UPDATE_TRAFFIC_DATA_PERIOD_SEC);
    err = esp_timer_start_periodic(updateTrafficTimer, UPDATE_TRAFFIC_DATA_PERIOD_SEC * 1000000);
    if (err != ESP_OK) return err;
    
    /* set a timer that goes off at next job */
    nextJobSecs = secsUntilNextJob();
    ESP_LOGI(TAG, "job timer set for %lld seconds from now", nextJobSecs);
    err = esp_timer_start_once(nextJobTimer, nextJobSecs * 1000000);
    if (err != ESP_OK) return err;

    return ESP_OK;
}

/**
 * @brief returns the number of seconds until a job is scheduled to occur.
 * 
 * @requires:
 *  - SNTP initialized, ie. gettimeofday is callable.
 * 
 * @returns A positive integer representing the number of seconds until a
 * job is scheduled to occur. If no job is scheduled, 0 is returned. If an
 * error occurred, -1 is returned. Other negative values are unexpected.
 */
static int64_t secsUntilNextJob(void)
{
    time_t currTime, earliestJobOfDay, earliestJobAfterCurrTime;
    earliestJobAfterCurrTime = SECONDS_IN_DAY;
    earliestJobOfDay = SECONDS_IN_DAY;
    bool foundAfter = false;
    struct tm localTime;

    /* get current time */
    if (time(&currTime) == -1)
    {
        ESP_LOGE(TAG, "failed to get time");
        return -1;
    }
    if (localtime_r(&currTime, &localTime) == NULL)
    {
        ESP_LOGE(TAG, "failed to get current time of day");
        return -1;
    }

    currTime = HOURS_TO_SECS(localTime.tm_hour) + MINS_TO_SECS(localTime.tm_min) + localTime.tm_sec;
    ESP_LOGI(TAG, "Seconds passed today: %lld", currTime);

    /* check for earliest job and earliest job after current time */
    if (CHECK_OTA_AVAILABLE_TIMES_SIZE == 0) 
    {
        ESP_LOGW(TAG, "No scheduled jobs");
        return 0;
    }

    for (int i = 0; i < CHECK_OTA_AVAILABLE_TIMES_SIZE; i++)
    {
        time_t jobTime = checkOTAAvailableTimes[i];
        if (jobTime < earliestJobOfDay)
        {
            earliestJobOfDay = jobTime;
        }
        if (jobTime > currTime &&
            jobTime < earliestJobAfterCurrTime)
        {
            foundAfter = true;
            earliestJobAfterCurrTime = jobTime;
        }
    }

    /* return seconds until next job */
    if (!foundAfter)
    {
        /* no jobs occur after currTime, thus schedule for next day */
        return SECONDS_IN_DAY - (currTime - earliestJobOfDay);
    }
    /* a job occurs after currTime */
    return earliestJobAfterCurrTime - currTime;
}


static void jobTimerCallback(void *arg)
{
    int64_t nextJobSecs;
    ESP_LOGI(TAG, "Job timer expired...");

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

    nextJobSecs = secsUntilNextJob();
    ESP_LOGI(TAG, "job timer set for %lld seconds from now", nextJobSecs);
    (void) esp_timer_start_once(nextJobTimer, nextJobSecs * 1000000); // nowhere for the error to go
}


static void updateDataTimerCallback(void *arg)
{
    ESP_LOGI(TAG, "Data timer expired...");
    
}