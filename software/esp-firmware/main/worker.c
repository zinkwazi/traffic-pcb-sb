/**
 * worker.c
 * 
 * This file contains task functions
 * that allow the application to be efficient.
 */

#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_https_ota.h"
#include "utilities.h"
#include "dots_commands.h"
#include "tomtom.h"
#include "led_locations.h"
#include "pinout.h"
#include "worker.h"
#include "led_registers.h"
#include "wifi.h"

#define DOTS_GLOBAL_CURRENT 0x25 // TODO: move this back to main

void setColor(uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t speed) {
    if (speed < 30) {
        *red = 0xFF;
        *green = 0x00;
        *blue = 0x00;
    } else if (speed < 60) {
        *red = 0x33;
        *green = 0x08;
        *blue = 0x00;
    } else {
        *red = 0x00;
        *green = 0x00;
        *blue = 0x08;
    }
}

void handleRefreshNorth(bool *aborted, QueueHandle_t I2CQueue, QueueHandle_t dotQueue, tomtomClient client) {
    DotCommand command;
    uint8_t red, green, blue;
    uint8_t speeds[NUM_LEDS];
    int speedsSize = NUM_LEDS + 1;
    ESP_LOGI(TAG, "Refreshing North...");
    *aborted = false;
    /* connect to API and query speeds */
    if (tomtomGetServerSpeeds(speeds, speedsSize, NORTH, client.httpHandle, CONFIG_HARDWARE_VERSION CONFIG_SERVER_FIRMWARE_VERSION, 5) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retrieve segment speeds from server");
        return;
    }
    for (int ndx = speedsSize - 1; ndx > 0; ndx--) {
        setColor(&red, &green, &blue, speeds[ndx]);
        if (dotsSetColor(I2CQueue, ndx, red, green, blue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
            dotsSetScaling(I2CQueue, ndx, 0xFF, 0xFF, 0xFF, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK)
        {
            ESP_LOGE(TAG, "failed to change led %d color", ndx);
        }
        if (xQueuePeek(dotQueue, &command, 0) == pdTRUE) {
            /* A new command has been issued, quick clear and abort command */
            ESP_LOGI(TAG, "Quick Clearing...");
            if (dotsReset(I2CQueue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
                dotsSetGlobalCurrentControl(I2CQueue, DOTS_GLOBAL_CURRENT, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK ||
                dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK) 
            {
                ESP_LOGE(TAG, "failed to reset dot matrices");
            }
            *aborted = true;
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD));
    }
}

void handleRefreshSouth(bool *aborted, QueueHandle_t I2CQueue, QueueHandle_t dotQueue, tomtomClient client) {
    DotCommand command;
    uint8_t red, green, blue;
    uint8_t speeds[NUM_LEDS];
    int speedsSize = NUM_LEDS + 1;
    ESP_LOGI(TAG, "Refreshing South...");
    *aborted = false;
    /* connect to API and query speeds */
    if (tomtomGetServerSpeeds(speeds, speedsSize, NORTH, client.httpHandle, CONFIG_HARDWARE_VERSION CONFIG_SERVER_FIRMWARE_VERSION, 5) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retrieve segment speeds from server");
        return;
    }
    for (int ndx = 1; ndx < speedsSize - 1; ndx++) {
        setColor(&red, &green, &blue, speeds[ndx]);
        if (dotsSetColor(I2CQueue, ndx, red, green, blue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
            dotsSetScaling(I2CQueue, ndx, 0xFF, 0xFF, 0xFF, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK)
        {
            ESP_LOGE(TAG, "failed to change led %d color", ndx);
        }
        if (xQueuePeek(dotQueue, &command, 0) == pdTRUE) {
            /* A new command has been issued, quick clear and abort command */
            ESP_LOGI(TAG, "Quick Clearing...");
            if (dotsReset(I2CQueue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
                dotsSetGlobalCurrentControl(I2CQueue, DOTS_GLOBAL_CURRENT, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK ||
                dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK) 
            {
                ESP_LOGE(TAG, "failed to reset dot matrices");
            }
            *aborted = true;
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD));
    }
}

/**
 * Toggles the error LED to indicate that
 * an issue requesting traffic data from TomTom
 * has occurred, which is likely due to an invalid
 * or overused api key.
 */
void tomtomErrorTimerCallback(void *params) {
    static int currentOutput = 0;
    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
    currentOutput = (currentOutput == 0) ? 1 : 0;
    gpio_set_level(ERR_LED_PIN, currentOutput);
}

/**
 * Accepts requests for dot updates off of
 * a queue, retrieves the dot's current speed,
 * then sends a command to the I2C gatekeeper
 * to update the color of the dot.
 */
void vDotWorkerTask(void *pvParameters) {
    QueueHandle_t dotQueue = ((struct dotWorkerTaskParams*) pvParameters)->dotQueue;
    QueueHandle_t I2CQueue = ((struct dotWorkerTaskParams*) pvParameters)->I2CQueue;
    char *apiKey = ((struct dotWorkerTaskParams*) pvParameters)->apiKey;
    bool *errorOccurred = ((struct dotWorkerTaskParams*) pvParameters)->errorOccurred;
    SemaphoreHandle_t errorOccurredMutex = ((struct dotWorkerTaskParams*) pvParameters)->errorOccurredMutex;
    tomtomClient client;
    if (tomtomInitClient(&client, apiKey) != ESP_OK) {
        if (!boolWithTestSet(errorOccurred, errorOccurredMutex)) {
            gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
            gpio_set_level(ERR_LED_PIN, 1);
        }
        for (;;) {}
    }
    while (tomtomInitClient(&client, apiKey) != ESP_OK) {
        vTaskDelay(RETRY_CREATE_HTTP_HANDLE_TICKS);
    }
    DotCommand dot;
    /* Wait for commands and execute them forever */
    bool prevCommandAborted = false;
    for (;;) {  // This task should never end
        if (ulTaskNotifyTake(pdTRUE, 0) == 1) {
            /* recieved an error from I2C gatekeeper */
            if (!boolWithTestSet(errorOccurred, errorOccurredMutex)) {
                gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
                gpio_set_level(ERR_LED_PIN, 1);
            }
        }
        /* wait for a command on the queue */
        while (xQueueReceive(dotQueue, &dot, INT_MAX) == pdFALSE) {}
        /* update led colors */
        switch (dot.type) {
            case REFRESH_NORTH:
                handleRefreshNorth(&prevCommandAborted, I2CQueue, dotQueue, client);
                break;
            case REFRESH_SOUTH:
                handleRefreshSouth(&prevCommandAborted, I2CQueue, dotQueue, client);
                break;
            case CLEAR_NORTH:
                if (prevCommandAborted) {
                    break;
                }
                ESP_LOGI(TAG, "Clearing North...");
                for (int ndx = NUM_LEDS; ndx > 0; ndx--) {
                    if (dotsSetColor(I2CQueue, ndx, 0x00, 0x00, 0x00, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK) {
                        ESP_LOGE(TAG, "failed to change led %d color", ndx);
                    }
                    vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
                }
                prevCommandAborted = false;
                break;
            case CLEAR_SOUTH:
                if (prevCommandAborted) {
                    break;
                }
                ESP_LOGI(TAG, "Clearing South...");
                for (int ndx = 1; ndx < NUM_LEDS + 1; ndx++) {
                    if (dotsSetColor(I2CQueue, ndx, 0x00, 0x00, 0x00, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK) {
                        ESP_LOGE(TAG, "failed to change led %d color", ndx);
                    }
                    vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
                }
                prevCommandAborted = false;
                break;
            case QUICK_CLEAR:
                ESP_LOGI(TAG, "Quick Clearing...");
                if (dotsReset(I2CQueue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
                    dotsSetGlobalCurrentControl(I2CQueue, DOTS_GLOBAL_CURRENT, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK ||
                    dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK) 
                {
                    ESP_LOGE(TAG, "failed to reset dot matrices");
                }
                prevCommandAborted = false;
                break;
            default:
                break;
        }
        
    }
    ESP_LOGE(TAG, "dot worker task is exiting! This should be impossible!");
    vTaskDelete(NULL); // exit safely (should never happen)
}

void vOTATask(void* pvParameters) {
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
            .url = CONFIG_FIRMWARE_UPGRADE_SERVER "/firmware/firmware" CONFIG_HARDWARE_VERSION ".bin",
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
    }
}