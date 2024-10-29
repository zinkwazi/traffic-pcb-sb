/**
 * worker.h
 * 
 * This file contains task functions
 * that allow the application to be efficient.
 */

#ifndef WORKER_H_
#define WORKER_H_

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "dots_commands.h"
#include "tomtom.h"

#define TAG "dot_worker"

#define RETRY_CREATE_HTTP_HANDLE_TICKS 500

struct DotCommand {
    uint16_t ledNum;
    Direction dir;
};

typedef struct DotCommand DotCommand;

struct dotWorkerTaskParams {
    /* Holds dot update requests for dot worker tasks */
    QueueHandle_t dotQueue; // holds DotCommand
    /* Holds commands for the I2C gatekeeper */
    QueueHandle_t I2CQueue; // holds I2CCommand
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
    struct requestResult storage = {
        .error = ESP_FAIL,
        .result = 0,
    };
    esp_http_client_handle_t tomtomHandle = tomtomCreateHttpHandle(&storage);
    while (tomtomHandle == NULL) {
        vTaskDelay(RETRY_CREATE_HTTP_HANDLE_TICKS);
        tomtomHandle = tomtomCreateHttpHandle(&storage);
    }
    DotCommand dot;
    uint speed = 0;
    /* Wait for commands and execute them forever */
    for (;;) {  // This task should never end
        if (xQueueReceive(dotQueue, &dot, INT_MAX) == pdFALSE) {
            continue;
        }
        if (tomtomRequestSpeed(&speed, tomtomHandle, &storage, dot.ledNum, dot.dir) != ESP_OK) {
            ESP_LOGE(TAG, "failed to request led %d speed from TomTom", dot.ledNum);
            continue;
        }
        if (speed < 30) {
            if (dotsSetColor(I2CQueue, dot.ledNum, 0xFF, 0x00, 0x00) != ESP_OK) {
                ESP_LOGE(TAG, "failed to change led %d color", dot.ledNum);
            }
        } else if (speed < 60) {
             if (dotsSetColor(I2CQueue, dot.ledNum, 0xFF, 0x55, 0x00) != ESP_OK) {
                ESP_LOGE(TAG, "failed to change led %d color", dot.ledNum);
            }
        } else {
             if (dotsSetColor(I2CQueue, dot.ledNum, 0x00, 0xFF, 0x00) != ESP_OK) {
                ESP_LOGE(TAG, "failed to change led %d color", dot.ledNum);
            }
        }
    }
    ESP_LOGE(TAG, "dot worker task is exiting! This should be impossible!");
    vTaskDelete(NULL); // exit safely (should never happen)
}


#endif /* WORKER_H_ */