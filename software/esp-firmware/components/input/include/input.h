/**
 * input.h
 * 
 * Created On: 9/6/2026
 * Author: Jaden Baptista
 * 
 * Contains button input functionality, generating commands for
 * the main task via the main queue and sends task notifications
 * to the OTA task based on the OTA button.
 */

#ifndef INPUT_H_6_21_25
#define INPUT_H_6_21_25

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef enum {
    MAIN_CMD_QUICK_DIR_BTN, // a short button press occurred
    MAIN_CMD_HOLD_DIR_BTN, // a long button press occurred
    MAIN_CMD_SCHEDULED, // a scheduled refresh is requested
    MAIN_CMD_TIMEOUT, // a timeout of the refresh timer occurred
    MAIN_CMD_ERROR, // indicates an error popping from the input queue
} MainCommand;

/**
 * A queue that the main task uses to receive commands.
 */
extern QueueHandle_t mainInputQueue; // holds mainCommand type.

esp_err_t initInput(TaskHandle_t otaTask);
esp_err_t incrementAbortCount(void);
esp_err_t decrementAbortCount(void);
bool abortCountZero(void);

#endif /* INPUT_H_6_21_25 */