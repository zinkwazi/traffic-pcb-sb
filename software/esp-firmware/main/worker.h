/**
 * worker.h
 * 
 * This file contains task functions
 * that allow the application to be efficient.
 */

#ifndef WORKER_H_
#define WORKER_H_

#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "utilities.h"
#include "dots_commands.h"
#include "tomtom.h"
#include "led_locations.h"
#include "pinout.h"
#include "worker.h"

#define TAG "dot_worker"

#define RETRY_CREATE_HTTP_HANDLE_TICKS 500
#define CHECK_ERROR_PERIOD_TICKS 500

struct DotCommand {
    uint16_t ledArrNum; // the array index of the location to query
    Direction dir;
};

typedef struct DotCommand DotCommand;

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

struct dotWorkerTaskParams {
    /* Holds dot update requests for dot worker tasks */
    QueueHandle_t dotQueue; // holds DotCommand
    /* Holds commands for the I2C gatekeeper */
    QueueHandle_t I2CQueue; // holds I2CCommand
    char *apiKey;
    bool *errorOccurred; // an indicator that an error has already occurred somewhere
    SemaphoreHandle_t errorOccurredMutex; // guards the shared static errorOccurred variable
};

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
        if (xQueueReceive(dotQueue, &dot, CHECK_ERROR_PERIOD_TICKS) == pdFALSE) {
            continue;
        }
        const LEDLoc *ledLoc = getLoc(dot.ledArrNum, dot.dir);
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
            red = 0xFF;
            green = 0x55;
            blue = 0x00;
        } else {
            red = 0x00;
            green = 0xFF;
            blue = 0x00;
        }
        /* update led colors */
        /* determine length of hardware LED array */
        int hardwareArrLen = 0;
        while (ledLoc->hardwareNums[hardwareArrLen] != 0) {
            hardwareArrLen++;
        }
        /* update led colors in the proper order */
        switch (dot.dir) {
            case NORTH:
                for (int ndx = hardwareArrLen - 1; ndx >= 0; ndx--) {
                    ESP_LOGI(TAG, "setting color of hardware num %d", ledLoc->hardwareNums[ndx]);
                    if (dotsSetColor(I2CQueue, ledLoc->hardwareNums[ndx], red, green, blue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK) {
                        ESP_LOGE(TAG, "failed to change led %d color", ledLoc->hardwareNums[ndx]);
                    }
                }
                break;
            case SOUTH:
                for (int ndx = 0; ndx < hardwareArrLen; ndx++) {
                    ESP_LOGI(TAG, "setting color of hardware num %d", ledLoc->hardwareNums[ndx]);
                    if (dotsSetColor(I2CQueue, ledLoc->hardwareNums[ndx], red, green, blue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK) {
                        ESP_LOGE(TAG, "failed to change led %d color", ledLoc->hardwareNums[ndx]);
                    }
                }
                break;
            default:
                ESP_LOGE(TAG, "worker encountered invalid direction");
                break;
        }
    }
    ESP_LOGE(TAG, "dot worker task is exiting! This should be impossible!");
    vTaskDelete(NULL); // exit safely (should never happen)
}


#endif /* WORKER_H_ */