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
#include "led_registers.h"

#define TAG "dot_worker"

#define RETRY_CREATE_HTTP_HANDLE_TICKS 500
#define CHECK_ERROR_PERIOD_TICKS 500

#define NUM_LEDS sizeof(LEDNumToReg) / sizeof(LEDNumToReg[0])

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
void tomtomErrorTimerCallback(void *params);

struct dotWorkerTaskParams {
    /* Holds dot update requests for dot worker tasks */
    QueueHandle_t dotQueue; // holds DotCommand
    /* Holds commands for the I2C gatekeeper */
    QueueHandle_t I2CQueue; // holds I2CCommand
    EventGroupHandle_t workerEvents; // holds indicators that workers are idle
    EventBits_t workerIdleBit; // this worker's workerEvents bit, indicating it is idle
    char *apiKey;
    bool *errorOccurred; // an indicator that an error has already occurred somewhere
    SemaphoreHandle_t errorOccurredMutex; // guards the shared static errorOccurred variable
};

void vDotWorkerTask(void *pvParameters);
void vOTATask(void* pvParameters);

#endif /* WORKER_H_ */