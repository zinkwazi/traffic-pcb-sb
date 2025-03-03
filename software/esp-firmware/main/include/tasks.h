/**
 * tasks.h
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

#include "led_matrix.h"
#include "pinout.h"
#include "app_errors.h"

#include "main_types.h"

#define TAG "tasks"

#define RETRY_CREATE_HTTP_HANDLE_TICKS 500
#define CHECK_ERROR_PERIOD_TICKS 500

esp_err_t createOTATask(TaskHandle_t *handle, const ErrorResources *errorResources);
void vOTATask(void* pvParameters);

#endif /* WORKER_H_ */