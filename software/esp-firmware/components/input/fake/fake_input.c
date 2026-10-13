/**
 * fake_input.c
 *
 * Contains a fake implementation of functions in input.h
 * to allow testing hardware dependencies.
 */
#include "sdkconfig.h"
#if defined(CONFIG_FAKE_INPUT)

#include "input.h"

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "app_err.h"

#define TAG "input"

#define INPUT_QUEUE_LENGTH (20)

QueueHandle_t mainInputQueue = NULL; // holds MainCommand type. Exposed for inter-task communication

static SemaphoreHandle_t abortCountSemaphore;

esp_err_t initInput(TaskHandle_t otaTask)
{
    if (NULL == otaTask) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (NULL != mainInputQueue) THROW_ERR(ESP_ERR_INVALID_STATE);

    mainInputQueue = xQueueCreate(INPUT_QUEUE_LENGTH, sizeof(MainCommand));
    if (NULL == mainInputQueue) THROW_ERR(ESP_FAIL);
    abortCountSemaphore = xSemaphoreCreateCounting(INPUT_QUEUE_LENGTH, 0);
    if (NULL == abortCountSemaphore) THROW_ERR(ESP_ERR_NO_MEM);

    return ESP_OK;
}

esp_err_t incrementAbortCount(void)
{
    if (xSemaphoreGive(abortCountSemaphore) != pdTRUE) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t decrementAbortCount(void)
{
    if (xSemaphoreTake(abortCountSemaphore, 0) != pdTRUE) return ESP_FAIL;
    return ESP_OK;
}

bool abortCountZero(void)
{
    return uxSemaphoreGetCount(abortCountSemaphore) == 0;
}

#endif /* CONFIG_FAKE_INPUT */
