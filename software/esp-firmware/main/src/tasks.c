/**
 * tasks.c
 * 
 * This file contains task functions
 * that allow the application to be efficient.
 */

#include "tasks.h"

#include <stdbool.h>
#include <stddef.h>

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "app_errors.h"

#include "indicators.h"
#include "utilities.h"
#include "wifi.h"

#define TAG "tasks"

/**
 * @brief Initializes the over-the-air (OTA) task, which is implemented by
 *        vOTATask.
 * 
 * @note This function creates shallow copies of parameters that will be 
 *       provided to the task in static memory. It assumes that only one of 
 *       this type of task will be created; any additional tasks will have 
 *       pointers to the same location in static memory.
 * 
 * @param handle A pointer to a handle which will refer to the created task
 *               if successful.
 * @param errorResources A pointer to an ErrorResources object. A deep copy
 *                       of the object will be created in static memory.
 *                       
 * @returns ESP_OK if the task was created successfully, otherwise ESP_FAIL.
 */
esp_err_t createOTATask(TaskHandle_t *handle, const ErrorResources *errorResources) {
    static ErrorResources taskErrorResources;
    BaseType_t success;
    /* input guards */
    if (errorResources == NULL ||
        errorResources->errMutex == NULL)
    {
        return ESP_FAIL;
    }
    /* copy parameters */
    taskErrorResources.err = errorResources->err;
    taskErrorResources.errMutex = errorResources->errMutex;
    taskErrorResources.errTimer = errorResources->errTimer;
    /* create OTA task */
    success = xTaskCreate(vOTATask, "OTATask", CONFIG_OTA_STACK,
                          &taskErrorResources, CONFIG_OTA_PRIO, handle);
    return (success == pdPASS) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Implements the over-the-air (OTA) task, which is responsible for
 *        handling user requests to update to the latest version of firmware.
 * 
 * @note To avoid runtime errors, the OTA task should only be created by the
 *       createOTATask function.
 * 
 * @param pvParameters A pointer to an ErrorResources object which should
 *                     remain valid through the lifetime of the task.
 */
void vOTATask(void* pvParameters) {
    ErrorResources *errRes = (ErrorResources *) pvParameters;
    esp_err_t err;
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, INT_MAX) == 0) 
        {
            continue; // block on notification timed out
        }
        // received a task notification indicating update firmware
        ESP_LOGI(TAG, "OTA update in progress...");
        (void) indicateOTAUpdate(); // allow update away from bad firmware
        esp_http_client_config_t https_config = {
            .url = FIRMWARE_UPGRADE_URL,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        esp_https_ota_config_t ota_config = {
            .http_config = &https_config,
        };
        err = esp_https_ota(&ota_config);
        if (err == ESP_OK) 
        {
            ESP_LOGI(TAG, "completed OTA update successfully!");
            (void) indicateOTASuccess(CONFIG_OTA_LEFT_ON_MS); // restart imminent anyway
            unregisterWifiHandler();
            esp_restart();
        }
        
        ESP_LOGI(TAG, "did not complete OTA update successfully!");
        err = indicateOTAFailure(errRes, CONFIG_OTA_LEFT_ON_MS);
        FATAL_IF_ERR(err, errRes);
        if (err != ESP_OK)
        {
            throwFatalError(errRes, false);
        }
    }
}