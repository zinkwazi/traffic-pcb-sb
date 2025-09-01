/**
 * input_queue.h
 * 
 * Contains interaction functions for a queue
 * which contains input for the main task from
 * various sources.
 */

#ifndef INPUT_QUEUE_H_82625
#define INPUT_QUEUE_H_82625

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"

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
extern QueueHandle_t inputQueue; // holds mainCommand type.

esp_err_t initInputQueue(void);
esp_err_t incrementAbortCount(void);
esp_err_t decrementAbortCount(void);
bool abortCountZero(void);

#endif /* INPUT_QUEUE_H_82625 */