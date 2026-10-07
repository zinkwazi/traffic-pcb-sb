/**
 * fake_input.c
 *
 * Contains a fake implementation of functions in input.h
 * to allow testing hardware dependencies.
 */
#include "sdkconfig.h"
#if defined(CONFIG_FAKE_INPUT)

#include "input.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_err.h"
#include "input_queue.h"

#define TAG "input"

esp_err_t initInput(TaskHandle_t otaTask)
{
    if (NULL == otaTask) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (NULL == mainInputQueue) THROW_ERR(ESP_ERR_INVALID_STATE);

    return ESP_OK;
}

#endif /* CONFIG_FAKE_INPUT */
