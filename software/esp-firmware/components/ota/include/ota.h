/**
 * ota.h
 * 
 * Contains over-the-air update functionality, handled through an OTA task.
 */

#ifndef OTA_H_3_27_25
#define OTA_H_3_27_25

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/freeRTOS.h"
#include "freertos/task.h"

#include "app_errors.h"

esp_err_t createOTATask(TaskHandle_t *handle, const ErrorResources *errorResources);
esp_err_t queryOTAUpdateAvailable(bool *available);

#endif /* OTA_H_3_27_25 */