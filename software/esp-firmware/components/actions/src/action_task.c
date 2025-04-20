/**
 * action_task.c
 * 
 * Contains the action task, which is a task that schedules actions to run at
 * particular times of the day. The task exists separate from the ESP Timer
 * task because it runs low priority actions, whereas the ESP Timer runs at a
 * very high priority.
 */

#include "action_task.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "app_errors.h"

#include "actions.h"

#define TAG "action_task"

#define ACTION_QUEUE_LEN (10)

/* The SNTP sync interval, in minutes */
#define SYNC_INTERVAL (120)

#define HOURS_TO_SECS(h) (h * 60 * 60)
#define MINS_TO_SECS(m) (m * 60)
#define SECONDS_IN_DAY (HOURS_TO_SECS(24))

static void vActionTask(void *pvParams);
static esp_err_t initActions(void);
static int64_t secsUntilNextAction(void);
static esp_err_t sendAction(Action action);
static void actionTimerCallback(void *arg);
static void updateDataTimerCallback(void *arg);

/* A queue on which actions will be sent to the action task */
static QueueHandle_t sActionQueue = NULL; // holds Action objects

static const esp_timer_create_args_t updateTrafficTimerCfg = {
    .callback = updateDataTimerCallback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "dataTimer",
};

static esp_timer_handle_t updateTrafficTimer = NULL;

static const esp_timer_create_args_t nextActionTimerCfg = {
    .callback = actionTimerCallback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "jobTimer",
};

static esp_timer_handle_t nextActionTimer = NULL;

/**
 * @brief Initializes the action task, which is implemented by vActionTask.
 * 
 * @note This function creates shallow copies of parameters that will be
 * provided to the task in static memory. It assumes that only one of this
 * type of task will be created. Any additional tasks will have pointers
 * to the same location in static memory.
 * 
 * @param[in] handle A pointer to a handle which will refer to the created task
 * if successful.
 *                       
 * @returns ESP_OK if the task was created successfully, otherwise ESP_FAIL.
 */
esp_err_t createActionTask(TaskHandle_t *handle)
{
    BaseType_t success;
    /* create OTA task */
    success = xTaskCreate(vActionTask, "ActionTask", CONFIG_ACTION_STACK,
                          NULL, CONFIG_ACTION_PRIO, handle);
    return (success == pdPASS) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Implements the action task, which is responsible for handling
 * various low-priority actions that are scheduled throughout the day.
 * 
 * @note The avoid runtime errors, the action task should only be created
 * by the createActionTask function.
 * 
 * @param[in] pvParams Unused.
 */
static void vActionTask(void *pvParams)
{
    BaseType_t success;
    esp_err_t err;

    /* initialization */
    sActionQueue = xQueueCreate(ACTION_QUEUE_LEN, sizeof(Action));
    if (sActionQueue == NULL) throwFatalError();
    if (initActions() != ESP_OK) throwFatalError();

    /* wait for actions off queue */
    while (true)
    {
        Action currAction;
        success = xQueueReceive(sActionQueue, &currAction, portMAX_DELAY);
        if (success != pdTRUE) throwFatalError();
        /* handle action */
        (void) handleAction(currAction);
        /* restart action timer */
        if (currAction == ACTION_UPDATE_DATA) continue;
        
        int64_t nextActionSecs = secsUntilNextAction();
        ESP_LOGI(TAG, "action timer set for %lld seconds from now", nextActionSecs);
        err = esp_timer_start_once(nextActionTimer, nextActionSecs * 1000000);
        if (err != ESP_OK) throwFatalError();
    }
    ESP_LOGE(TAG, "Action task is exiting!");
    throwFatalError();
}

/**
 * @brief Initializes SNTP and timers to run jobs at particular times of day
 *        and starts both the data update and job timer.
 * 
 * @requires:
 *  - esp_timer library initialized by esp_timer_init.
 * 
 * @returns ESP_OK if successful.
 */
static esp_err_t initActions(void)
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
    err = esp_timer_create(&nextActionTimerCfg, &nextActionTimer);
    if (err != ESP_OK) return err;

    /* start timers */
    ESP_LOGI(TAG, "traffic timer set for %lld seconds from now", getUpdateTrafficDataPeriodSec());
    err = esp_timer_start_periodic(updateTrafficTimer, getUpdateTrafficDataPeriodSec() * 1000000);
    if (err != ESP_OK) return err;
    
    /* set a timer that goes off at next job */
    nextJobSecs = secsUntilNextAction();
    ESP_LOGI(TAG, "job timer set for %lld seconds from now", nextJobSecs);
    err = esp_timer_start_once(nextActionTimer, nextJobSecs * 1000000);
    if (err != ESP_OK) return err;

    return ESP_OK;
}

/**
 * @brief returns the number of seconds until an action is scheduled to occur.
 * 
 * @requires:
 *  - SNTP initialized, ie. gettimeofday is callable.
 * 
 * @returns A positive integer representing the number of seconds until a
 * job is scheduled to occur. If no job is scheduled, 0 is returned. If an
 * error occurred, -1 is returned. Other negative values are unexpected.
 */
static int64_t secsUntilNextAction(void)
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
    if (getCheckOTAAvailableTimes() == 0) 
    {
        ESP_LOGW(TAG, "No scheduled jobs");
        return 0;
    }

    for (int i = 0; i < getCheckOTAAvailableTimesSize(); i++)
    {
        time_t jobTime = getCheckOTAAvailableTimes()[i];
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

/**
 * @brief Sends an action to the action task, who will perform the action
 * immediately at a low priority.
 * 
 * @param[in] action The action for the action task to perform.
 * 
 * @returns ESP_OK if successful.
 */
static esp_err_t sendAction(Action action)
{
    BaseType_t success;
    if (sActionQueue == NULL) return ESP_FAIL;
    success = xQueueSend(sActionQueue, &action, portMAX_DELAY);
    if (success != pdTRUE) return ESP_FAIL;
    return ESP_OK;
}

static void actionTimerCallback(void *arg)
{
    ESP_LOGI(TAG, "Action timer expired...");
    if (sendAction(ACTION_QUERY_OTA) != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to send action");
    }
}

static void updateDataTimerCallback(void *arg)
{
    ESP_LOGI(TAG, "Data timer expired...");
    if (sendAction(ACTION_UPDATE_DATA) != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to send action");
    }
}

