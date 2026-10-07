/**
 * input.h
 * 
 * Created On: 9/6/2026
 * Author: Jaden Baptista
 * 
 * Contains button input functionality. This
 * generates commands for the main task via the queue
 * of input_queue.h based on the toggle button. It also sends task
 * notifications to the OTA task based on the OTA button.
 */

#ifndef INPUT_H_6_21_25
#define INPUT_H_6_21_25

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t initInput(TaskHandle_t otaTask);

#endif /* INPUT_H_6_21_25 */