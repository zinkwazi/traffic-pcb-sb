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
    EventGroupHandle_t workerEvents = ((struct dotWorkerTaskParams*) pvParameters)->workerEvents;
    EventBits_t workerIdleBit = ((struct dotWorkerTaskParams*) pvParameters)->workerIdleBit;
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
    uint speed, red, green, blue;
    speed = 0;
    /* Wait for commands and execute them forever */
    for (;;) {  // This task should never end
        if (ulTaskNotifyTake(pdTRUE, 0) == 1) {
            /* recieved an error from I2C gatekeeper */
            if (!boolWithTestSet(errorOccurred, errorOccurredMutex)) {
                gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
                gpio_set_level(ERR_LED_PIN, 1);
            }
        }
        if (xQueueReceive(dotQueue, &dot, 0) == pdFALSE) {
            /* queue is empty, indicate that this task is idle */
            xEventGroupSetBits(workerEvents, workerIdleBit);
            /* wait for a command on the queue */
            while (xQueueReceive(dotQueue, &dot, INT_MAX) == pdFALSE) {}
            xEventGroupClearBits(workerEvents, workerIdleBit);
        }
        const LEDLoc *ledLoc = getLoc(dot.ledArrNum, dot.dir);
        /* connect to API and query speed */
        if (tomtomRequestSpeed(&speed, &client, ledLoc->latitude, ledLoc->longitude, CONFIG_NUM_RETRY_HTTP_REQUEST) != ESP_OK) {
            switch (dot.dir) {
                case NORTH:
                    ESP_LOGE(TAG, "failed to request northbound led location index %d speed from TomTom", dot.ledArrNum);
                    break;
                case SOUTH:
                    ESP_LOGE(TAG, "failed to request southbound led location index %d speed from TomTom", dot.ledArrNum);
                    break;
                default:
                    ESP_LOGE(TAG, "failed to request (unknown direction) led location index %d speed from TomTom", dot.ledArrNum);
                    break;
            }
            /* start error timer */
            if (!boolWithTestSet(errorOccurred, errorOccurredMutex)) {
                esp_timer_create_args_t timerArgs = {
                    .callback = tomtomErrorTimerCallback,
                    .arg = NULL,
                    .dispatch_method = ESP_TIMER_ISR,
                    .name = "errorTimer",
                };
                esp_timer_handle_t timer;
                if (esp_timer_create(&timerArgs, &timer) != ESP_OK || 
                    esp_timer_start_periodic(timer, CONFIG_ERROR_PERIOD * 1000) != ESP_OK) {
                    ESP_LOGE(TAG, "failed to start TomTom error timer");
                    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
                    gpio_set_level(ERR_LED_PIN, 1);
                }
            }
            continue;
        }
        /* determine correct color */
        if (speed < 30) {
            red = 0xFF;
            green = 0x00;
            blue = 0x00;
        } else if (speed < 60) {
            red = 0x00;
            green = 0x00;
            blue = 0xFF;
        } else {
            red = 0x00;
            green = 0xFF;
            blue = 0x00;
        }
        /* update led colors */
        /* determine length of hardware LED array */
        int hardwareArrLen = 1;
        int startNdx = 0;
        if (ledLoc->hardwareNums[0] < 0) {
            hardwareArrLen = -ledLoc->hardwareNums[0];
            startNdx = 1;
        }
        /* update led colors in the proper order */
        switch (dot.dir) {
            case NORTH:
                for (int ndx = startNdx + hardwareArrLen - 1; ndx >= startNdx; ndx--) {
                    if (dotsSetColor(I2CQueue, ledLoc->hardwareNums[ndx], red, green, blue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK) {
                        ESP_LOGE(TAG, "failed to change led %d color", ledLoc->hardwareNums[ndx]);
                    }
                }
                break;
            case SOUTH:
                for (int ndx = startNdx; ndx < startNdx + hardwareArrLen; ndx++) {
                    if (dotsSetColor(I2CQueue, ledLoc->hardwareNums[ndx], red, green, blue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK) {
                        ESP_LOGE(TAG, "failed to change led %d color", ledLoc->hardwareNums[ndx]);
                    }
                }
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