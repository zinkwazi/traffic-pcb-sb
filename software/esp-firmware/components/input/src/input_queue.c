/**
 * input_queue.c
 * 
 * Contains interaction functions for a queue
 * which contains input for the main task from
 * various sources.
 */

#include "input_queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"

#define TAG "input_queue"

#define INPUT_QUEUE_LENGTH (20)


QueueHandle_t inputQueue = NULL; // holds MainCommand type. Exposed for inter-task communication

/* Keeps track of the number of unhandled commands on the main queue that cause an abort of the previous refresh */
static SemaphoreHandle_t abortCountSemaphore;

/**
 * Initializes the input queue.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if the queue was already initialized.
 * ESP_FAIL if the queue could not be initialized.
 */
esp_err_t initInputQueue(void)
{
    if (NULL != inputQueue) return ESP_ERR_INVALID_STATE;
    inputQueue = xQueueCreate(INPUT_QUEUE_LENGTH, sizeof(MainCommand));
    if (NULL == inputQueue) return ESP_FAIL;
    abortCountSemaphore = xSemaphoreCreateCounting(INPUT_QUEUE_LENGTH, 0);
    if (NULL == abortCountSemaphore) return ESP_ERR_NO_MEM;
    return ESP_OK;
}

/**
 * Increments the abort count, which should happen when a command
 * to refresh the LEDs is added to the main queue (ie. short button press).
 * 
 * @note This count is used to keep track of the number of unprocessed
 * abortable commands on the queue, such that they are all aborted except
 * the last. It must be incremented only BEFORE the command is added to the
 * queue to avoid unexpected behavior (ex. aborting the command just added).
 * 
 * @note This function is not ISR safe.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t incrementAbortCount(void)
{
    if (xSemaphoreGive(abortCountSemaphore) != pdTRUE) {
        ESP_LOGE(TAG, "failed to increment abort count semaphore");
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * Decrements the abort count. As this should always be in sync with
 * the command queue, it is an error if this fails.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t decrementAbortCount(void)
{
    if (xSemaphoreTake(abortCountSemaphore, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to decrement abort count semaphore");
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * Checks whether the abort count is zero or not, corresponding
 * to whether a command exists on the queue that should cause an abort.
 * 
 * @returns True if the count is zero, otherwise False.
 */
bool abortCountZero(void)
{
    return uxSemaphoreGetCount(abortCountSemaphore) == 0;
}