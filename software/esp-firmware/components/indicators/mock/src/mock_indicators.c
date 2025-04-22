/**
 * mock_indicators.h
 * 
 * Mocks indicator.h functions, controlled by mock_indicators.h.
 */

#ifdef CONFIG_DISABLE_TESTING_FEATURES
#error "Mock indicators component included in non-testing build!"
#endif

#include "mock_indicators.h"
#include "indicators.h"

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"

#include "main_types.h"

#define TAG "mock_indicators"

static TaskHandle_t wakeTask; // the task to wake up when an indicator call is made
static MockIndicatorCall *recording; // dynamically allocated array of function calls
static size_t recordingSize; // the number of elements that can be stored in recording
static size_t recordingLen; // the number of elements currently stored in recording
static bool recordingOverflow; // whether too many calls were made or not

/**
 * @brief Allocates an array of MockIndicatorCall objects of size len to record
 * the order of function calls to indicators.h functions.
 * 
 * @requires:
 * - mockIndicatorsDestroyRecording called after recording is processed.
 * 
 * @returns ESP_OK if successfully allocated, otherwise ESP_FAIL.
 */
esp_err_t mockIndicatorsStartRecording(size_t len)
{
    recording = malloc(len * sizeof(MockIndicatorCall));
    if (recording == NULL) return ESP_FAIL;
    recordingSize = len;
    recordingLen = 0;
    recordingOverflow = false;
    return ESP_OK;
}

/**
 * @brief Returns the ndx'th indicators.h function call.
 * 
 * @returns The ndx'th indicators.h function call if successful. If there is
 * no ndx'th call, then INDICATE_RECORDING_END is returned. If the ndx is out
 * of bounds, then INDICATE_RECORDING_OOB (out of bounds) is returned. This
 * likely means that the recording array is not large enough.
 */
MockIndicatorCall mockIndicatorsGetRecording(size_t ndx)
{
    if (ndx > recordingSize) return INDICATE_RECORDING_OOB;
    if (ndx >= recordingLen) return INDICATE_RECORDING_END;
    return recording[ndx];
}

bool mockIndicatorsRecordingOverflowed(void)
{
    return recordingOverflow;
}

void mockIndicatorsDestroyRecording(void)
{
    free(recording);
    recording = NULL;
    recordingLen = 0;
    recordingSize = 0;
}

esp_err_t indicateWifiConnected(void)
{
    ESP_LOGI(TAG, "indicateWifiConnected called");
    if (recordingLen == recordingSize)
    {
        ESP_LOGW(TAG, "Failed to record indicateWifiConnected");
        recordingOverflow = true;
        return ESP_OK; // mock return
    }

    recording[recordingLen] = INDICATE_WIFI_CONNECTED;
    recordingLen++;
    return ESP_OK; // mock return
}

esp_err_t indicateWifiNotConnected(void)
{
    ESP_LOGI(TAG, "indicateWifiNotConnected called");
    if (recordingLen == recordingSize)
    {
        ESP_LOGW(TAG, "Failed to record indicateWifiNotConnected");
        recordingOverflow = true;
        return ESP_OK;
    }

    recording[recordingLen] = INDICATE_WIFI_NOT_CONNECTED;
    recordingLen++;
    return ESP_OK;
}

esp_err_t indicateOTAAvailable(void)
{
    ESP_LOGI(TAG, "indicateOTAAvailable called");
    if (recordingLen == recordingSize)
    {
        ESP_LOGW(TAG, "Failed to record indicateOTAAvailable");
        recordingOverflow = true;
        return ESP_OK;
    }

    recording[recordingLen] = INDICATE_OTA_AVAILABLE;
    recordingLen++;
    return ESP_OK;
}

esp_err_t indicateOTAUpdate(void)
{
    ESP_LOGI(TAG, "indicateOTAUpdate called");
    if (recordingLen == recordingSize)
    {
        ESP_LOGW(TAG, "Failed to record indicateOTAUpdate");
        recordingOverflow = true;
        return ESP_OK;
    }

    recording[recordingLen] = INDICATE_OTA_UPDATE;
    recordingLen++;
    return ESP_OK;
}

esp_err_t indicateOTAFailure(int32_t delay)
{
    ESP_LOGI(TAG, "indicateOTAFailure called");
    if (recordingLen == recordingSize)
    {
        ESP_LOGW(TAG, "Failed to record indicateOTAFailure");
        recordingOverflow = true;
        return ESP_OK;
    }

    recording[recordingLen] = INDICATE_OTA_FAILURE;
    recordingLen++;
    return ESP_OK;
}

esp_err_t indicateOTASuccess(int32_t delay)
{
    ESP_LOGI(TAG, "indicateOTASuccess called");
    if (recordingLen == recordingSize)
    {
        ESP_LOGW(TAG, "Failed to record indicateOTASuccess");
        recordingOverflow = true;
        return ESP_OK;
    }

    recording[recordingLen] = INDICATE_OTA_SUCCESS;
    recordingLen++;
    return ESP_OK;
}

esp_err_t indicateNorthbound(void)
{
    ESP_LOGI(TAG, "indicateNorthbound called");
    if (recordingLen == recordingSize)
    {
        ESP_LOGW(TAG, "Failed to record indicateNorthbound");
        recordingOverflow = true;
        return ESP_OK;
    }

    recording[recordingLen] = INDICATE_NORTHBOUND;
    recordingLen++;
    return ESP_OK;
}

esp_err_t indicateSouthbound(void)
{
    ESP_LOGI(TAG, "indicateSouthbound called");
    if (recordingLen == recordingSize)
    {
        ESP_LOGW(TAG, "Failed to record indicateSouthbound");
        recordingOverflow = true;
        return ESP_OK;
    }

    recording[recordingLen] = INDICATE_SOUTHBOUND;
    recordingLen++;
    return ESP_OK;
}

esp_err_t indicateDirection(Direction dir)
{
    ESP_LOGI(TAG, "indicateDirection called");
    if (recordingLen == recordingSize)
    {
        ESP_LOGW(TAG, "Failed to record indicateDirection");
        recordingOverflow = true;
        return ESP_OK;
    }

    recording[recordingLen] = INDICATE_DIRECTION;
    recordingLen++;
    return ESP_OK;
}