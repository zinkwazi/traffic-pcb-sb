/**
 * input.c
 *
 * Contains button input functionality. The button ISRs
 * here only capture edges; debouncing and press detection
 * are deferred to the button tasks of input_tasks.h.
 */

#include "sdkconfig.h"
#if !defined(CONFIG_FAKE_INPUT)

#include "input.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "app_err.h"
#include "input_queue.h"
#include "input_tasks.h"
#include "pinout.h"

#define TAG "input"

static esp_err_t initDirectionButton(void);
static esp_err_t initOTAButton(void);
static void otaButtonISR(void *params);
static void toggleButtonISR(void *params);

/**
 * Initializes all input buttons on the board, creating the
 * button tasks before enabling the button ISRs that feed them.
 *
 * @note initInputQueue and gpio_install_isr_service must
 * be called before this function.
 *
 * @param otaTask A handle to the OTA task, which will be sent
 * task notifications when the OTA button is pressed.
 *
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if the main input queue is not initialized.
 */
esp_err_t initInput(TaskHandle_t otaTask)
{
    esp_err_t err;

    if (NULL == otaTask) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (NULL == mainInputQueue) THROW_ERR(ESP_ERR_INVALID_STATE);

    /* the ISRs post to the tasks' event queues, so the tasks must exist first */
    err = createOTAButtonTask(NULL, CONFIG_INPUT_PRIO, otaTask);
    if (err != ESP_OK) THROW_ERR(err);
    err = createToggleButtonTask(NULL, CONFIG_INPUT_PRIO, mainInputQueue);
    if (err != ESP_OK) THROW_ERR(err);

    err = initDirectionButton();
    if (err != ESP_OK) return err;
    err = initOTAButton();
    if (err != ESP_OK) return err;

    return ESP_OK;
}

static esp_err_t initDirectionButton(void)
{
    esp_err_t err;

    err = gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_set_intr_type(T_SW_PIN, GPIO_INTR_ANYEDGE);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_isr_handler_add(T_SW_PIN, toggleButtonISR, NULL);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_intr_enable(T_SW_PIN);
    if (err != ESP_OK) THROW_ERR(err);

    return ESP_OK;
}

static esp_err_t initOTAButton(void)
{
    esp_err_t err;

    err = gpio_set_pull_mode(IO_SW_PIN, GPIO_PULLUP_ONLY);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_pullup_en(IO_SW_PIN);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_set_direction(IO_SW_PIN, GPIO_MODE_INPUT);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_set_intr_type(IO_SW_PIN, GPIO_INTR_ANYEDGE);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_isr_handler_add(IO_SW_PIN, otaButtonISR, NULL);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_intr_enable(IO_SW_PIN);
    if (err != ESP_OK) THROW_ERR(err);

    return ESP_OK;
}

/**
 * @brief Interrupt service routine that handles OTA button presses,
 * with processing deferred to the ota button debounce task.
 */
static void otaButtonISR(void *params)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const ButtonEvent event = {
        .edgeDetected = true,
        .pinHigh = (bool) gpio_get_level(IO_SW_PIN),
    };

    gpio_intr_disable(IO_SW_PIN);
    (void) xQueueSendToBackFromISR(otaButtonEventQueue, &event, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Interrupt service routine that handles toggle button presses,
 * with processing deferred to the toggle button debounce task.
 */
static void toggleButtonISR(void *params)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const ButtonEvent event = {
        .edgeDetected = true,
        .pinHigh = (bool) gpio_get_level(T_SW_PIN),
    };

    gpio_intr_disable(T_SW_PIN);
    (void) xQueueSendToBackFromISR(toggleButtonEventQueue, &event, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

#endif /* CONFIG_FAKE_INPUT */
