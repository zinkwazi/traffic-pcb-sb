/**
 * traffic_data.c
 * 
 * Contains thread-safe getters and setters for the current traffic data. In
 * this context, thread-safe means that only one task has ownership of either
 * all current or all typical traffic data at a time. Ownership allows the task 
 * to use the functions below and nothing more.
 */

#include "traffic_data.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "main_types.h"
#include "api_connect.h"
#include "led_registers.h"
#include "app_nvs.h"

static LEDData sCurrentNorthData[MAX_NUM_LEDS_REG];
static LEDData sCurrentSouthData[MAX_NUM_LEDS_REG];
static LEDData sTypicalNorthData[MAX_NUM_LEDS_REG];
static LEDData sTypicalSouthData[MAX_NUM_LEDS_REG];

/* a mutex guarding access to current traffic data */
static SemaphoreHandle_t sCurrentDataMutex = NULL;
/* a mutex guarding access to typical traffic data */
static SemaphoreHandle_t sTypicalDataMutex = NULL;

static bool accessAllowed(SpeedCategory category);
static LEDData *getTargetArray(Direction dir, SpeedCategory category);

/**
 * @brief Initializes traffic data ownership mechanism.
 * 
 * @note This does not initialize data. Data must be initialized manually
 * by calls to updateTrafficData after borrowTrafficData is called.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_NO_MEM if there was not enough FreeRTOS memory.
 */
esp_err_t initTrafficData(void)
{
    sCurrentDataMutex = xSemaphoreCreateMutex();
    if (sCurrentDataMutex == NULL) return ESP_ERR_NO_MEM;
    sTypicalDataMutex = xSemaphoreCreateMutex();
    if (sTypicalDataMutex == NULL) return ESP_ERR_NO_MEM;
    return ESP_OK;
}

/**
 * @brief Allows the calling task to take ownership of traffic data until
 * releaseTrafficData is called. This allows the task to use the functions
 * in traffic_data.h without receiving ESP_ERR_NOT_ALLOWED.
 * 
 * @requires:
 * - initTrafficData called.
 * 
 * @param[in] category The category of the traffic data to borrow.
 * @param[in] xTicksToWait The time in ticks to wait for the mutex to become
 * available.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if requirement 1 is not satisfied.
 * ESP_ERR_TIMEOUT if a timeout occurred.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t borrowTrafficData(SpeedCategory category, TickType_t xTicksToWait)
{
    BaseType_t success;
    if (sCurrentDataMutex == NULL) return ESP_ERR_INVALID_STATE;
    if (sTypicalDataMutex == NULL) return ESP_ERR_INVALID_STATE;

    switch (category)
    {
        case LIVE:
            success = xSemaphoreTake(sCurrentDataMutex, xTicksToWait);
            break;
        case TYPICAL:
            success = xSemaphoreTake(sTypicalDataMutex, xTicksToWait);
            break;
        default:
            return ESP_FAIL;
    }

    if (success != pdTRUE) return ESP_ERR_TIMEOUT;
    return ESP_OK;
}

/**
 * @brief Allows the calling task to give up ownership of traffic data previously
 * acquired by calling borrowTrafficData.
 * 
 * @requires:
 * - initTrafficData called.
 * 
 * @param[in] category The category of the traffic data to release.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if requirement 1 is not satisfied.
 * ESP_ERR_NOT_ALLOWED if this task does not have ownership over traffic data.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t releaseTrafficData(SpeedCategory category)
{
    BaseType_t success;
    if (sCurrentDataMutex == NULL) return ESP_ERR_INVALID_STATE;
    if (sTypicalDataMutex == NULL) return ESP_ERR_INVALID_STATE;

    switch (category)
    {
        case LIVE:
            success = xSemaphoreGive(sCurrentDataMutex);
            break;
        case TYPICAL:
            success = xSemaphoreGive(sTypicalDataMutex);
            break;
        default:
            return ESP_FAIL;
    }

    if (success != pdTRUE) return ESP_ERR_NOT_ALLOWED;
    return ESP_OK;
}

/**
 * @brief Updates a particular type of traffic data to the provided data.
 * 
 * @requires:
 * - Ownership of traffic data by calling borrowTrafficData.
 * 
 * @param[in] data The data to be copied to traffic data.
 * @param[in] dataSize The size of the data array, which must be greater than
 * or equal to MAX_NUM_LEDS_REG.
 * @param[in] dir THe direction of the traffic data that will be updated.
 * @param[in] category The category of the traffic data that will be updated.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_NOT_ALLOWED if this task does not have ownership over traffic data.
 * ESP_ERR_TIMEOUT if a timeout occurred waiting for data from the server.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t updateTrafficData(LEDData data[], size_t dataSize, Direction dir, SpeedCategory category)
{
    esp_err_t err;
    LEDData *targetArr;
    /* input guards */
    if (data == NULL) return ESP_ERR_INVALID_ARG;
    if (dataSize < MAX_NUM_LEDS_REG) return ESP_ERR_INVALID_ARG;
    if (!accessAllowed(category)) return ESP_ERR_NOT_ALLOWED;

    /* update data */
    targetArr = getTargetArray(dir, category);
    if (targetArr == NULL) return ESP_FAIL;

    for (size_t i = 0; i < MAX_NUM_LEDS_REG; i++)
    {
        targetArr[i] = data[i];
    }

    /* update NVS data */
    err = storeSpeedsToNVS(targetArr, dir, category);
    if (err != ESP_OK) return ESP_FAIL;

    return ESP_OK;
}

/**
 * @brief Copies a particular type of traffic data.
 * 
 * @requires:
 * - Ownership of traffic data by calling borrowTrafficData.
 * 
 * @param[out] out The location to copy traffic data to.
 * @param[in] outLen The length of the out array, which must be larger than
 * MAX_NUM_LEDS_REG.
 * @param[in] dir The direction of the data to copy.
 * @param[in] category The category of data to copy.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_NOT_ALLOWED if this task does not have ownership over traffic data.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t copyTrafficData(LEDData out[], size_t outLen, Direction dir, SpeedCategory category)
{
    LEDData *targetArr;
    /* input guards */
    if (out == NULL) return ESP_ERR_INVALID_ARG;
    if (outLen < MAX_NUM_LEDS_REG) return ESP_ERR_INVALID_ARG;
    if (!accessAllowed(category)) return ESP_ERR_NOT_ALLOWED;

    /* copy data */
    targetArr = getTargetArray(dir, category);
    if (targetArr == NULL) return ESP_FAIL;

    for (size_t i = 0; i < MAX_NUM_LEDS_REG; i++)
    {
        out[i] = targetArr[i];
    }
    
    return ESP_OK;
}

/**
 * @brief Determines whether the calling task is allowed to access the traffic
 * data of the provided category.
 * 
 * @param[in] category The category of traffic data.
 * 
 * @returns True if the calling task is the current owner of the guard mutex,
 * otherwise false.
 */
static bool accessAllowed(SpeedCategory category)
{
    if (sCurrentDataMutex == NULL) return false;
    if (sTypicalDataMutex == NULL) return false;
    switch (category)
    {
        case LIVE:
            if (xSemaphoreGetMutexHolder(sCurrentDataMutex) == xTaskGetCurrentTaskHandle())
            {
                return true;
            }
            return false;
        case TYPICAL:
            if (xSemaphoreGetMutexHolder(sTypicalDataMutex) == xTaskGetCurrentTaskHandle())
            {
                return true;
            }
            return false;
        default:
            break;
    }
    return false;
}

/**
 * @brief Determines the target traffic array corresponding to the given arguments.
 * 
 * @param[in] dir The direction of the target array.
 * @param[in] category The category of the target array.
 * 
 * @returns A pointer to the target array, otherwise NULL.
 */
static LEDData *getTargetArray(Direction dir, SpeedCategory category)
{
    switch (category)
    {
        case LIVE:
            switch (dir)
            {
                case NORTH:
                    return sCurrentNorthData;
                case SOUTH:
                    return sCurrentSouthData;
                default:
                    return NULL;
            }
            break;
        case TYPICAL:
            switch (dir)
            {
                case NORTH:
                    return sTypicalNorthData;
                case SOUTH:
                    return sTypicalSouthData;
                default:
                    return NULL;
            }
            break;
        default:
            return NULL;
    }
    return NULL;
}