/**
 * tasks.c
 * 
 * This file contains task functions
 * that allow the application to be efficient.
 */
#include "tasks.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"

#include "pinout.h"
#include "led_registers.h"
#include "api_connect.h"
#include "animations.h"

#include "main_types.h"
#include "wifi.h"
#include "utilities.h"

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
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, INT_MAX) == 0) {
            continue; // block on notification timed out
        }
        // received a task notification indicating update firmware
        ESP_LOGI(TAG, "OTA update in progress...");
        gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT);
        gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT);
        gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT);
        gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(LED_NORTH_PIN, 1);
        gpio_set_level(LED_EAST_PIN, 1);
        gpio_set_level(LED_SOUTH_PIN, 1);
        gpio_set_level(LED_WEST_PIN, 1);
        esp_http_client_config_t https_config = {
            .url = FIRMWARE_UPGRADE_URL,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        esp_https_ota_config_t ota_config = {
            .http_config = &https_config,
        };
        esp_err_t ret = esp_https_ota(&ota_config);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "completed OTA update successfully!");
            unregisterWifiHandler();
            esp_restart();
        }
        
        ESP_LOGI(TAG, "did not complete OTA update successfully!");
        throwHandleableError(errRes, false);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_OTA_LEFT_ON_MS)); // leave LEDs on for a bit
        gpio_set_level(LED_NORTH_PIN, 0);
        gpio_set_level(LED_EAST_PIN, 0);
        gpio_set_level(LED_SOUTH_PIN, 0);
        gpio_set_level(LED_WEST_PIN, 0);
        resolveHandleableError(errRes, false, false);
    }
}