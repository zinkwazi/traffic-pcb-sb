/**
 * ota.h
 * 
 * Contains over-the-air update functionality, handled through an OTA task.
 */

#ifndef OTA_H_3_27_25
#define OTA_H_3_27_25

#include "esp_err.h"
#include "freertos/freeRTOS.h"

#include "app_errors.h"

esp_err_t createOTATask(TaskHandle_t *handle, const ErrorResources *errorResources);

#endif /* OTA_H_3_27_25 */