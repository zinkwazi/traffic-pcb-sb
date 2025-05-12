/**
 * ota.h
 * 
 * Contains over-the-air update functionality, handled through an OTA task.
 */

#ifndef OTA_H_3_27_25
#define OTA_H_3_27_25

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_errors.h"

TaskHandle_t getOTATask(void);
esp_err_t createOTATask(TaskHandle_t *handle);
esp_err_t queryOTAUpdateAvailable(bool *available, bool *patch);

#endif /* OTA_H_3_27_25 */