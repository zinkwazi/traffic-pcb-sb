/**
 * strobe_task.h
 * 
 * Contains functionality to create the strobe task. Separate from strobe.h
 * in order to minimize the risk of modifying the strobe command queue handle.
 */

#ifndef STROBE_TASK_H_4_5_25
#define STROBE_TASK_H_4_5_25

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "app_errors.h"

QueueHandle_t getStrobeQueue(void);
esp_err_t acquireStrobeQueueMutex(TickType_t blockTime);
esp_err_t releaseStrobeQueueMutex(void);
esp_err_t createStrobeTask(TaskHandle_t *handle);

#endif /* STROBE_TASK_H_4_5_25 */