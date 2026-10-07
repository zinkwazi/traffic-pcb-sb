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

esp_err_t initInput(TaskHandle_t otaTask)
{
    return ESP_OK;
}

esp_err_t enableQuickDirButton(void)
{
    return ESP_OK;
}

esp_err_t disableQuickDirButton(void)
{
    return ESP_OK;
}

esp_err_t enableHoldDirButton(void)
{
    return ESP_OK;
}

esp_err_t disableHoldDirButton(void)
{
    return ESP_OK;
}

esp_err_t enableOTAButton(void)
{
    return ESP_OK;
}

esp_err_t disableOTAButton(void)
{
    return ESP_OK;
}

#endif /* CONFIG_FAKE_INPUT */
