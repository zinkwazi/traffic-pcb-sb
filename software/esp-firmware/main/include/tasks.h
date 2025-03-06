/**
 * tasks.h
 * 
 * This file contains task functions
 * that allow the application to be efficient.
 */

#ifndef WORKER_H_
#define WORKER_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_errors.h"

#define RETRY_CREATE_HTTP_HANDLE_TICKS 500
#define CHECK_ERROR_PERIOD_TICKS 500

esp_err_t createOTATask(TaskHandle_t *handle, const ErrorResources *errorResources);
void vOTATask(void* pvParameters);

#endif /* WORKER_H_ */