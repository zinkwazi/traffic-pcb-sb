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
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "app_err.h"
#include "input_tasks.h"
#include "pinout.h"

#define TAG "input"

#define INPUT_QUEUE_LENGTH (20)

QueueHandle_t mainInputQueue = NULL; // holds MainCommand type. Exposed for inter-task communication

/* Keeps track of the number of unhandled commands on the main queue that cause an abort of the previous refresh */
static SemaphoreHandle_t abortCountSemaphore;

static esp_err_t initDirectionButton(void);
static esp_err_t initOTAButton(void);
static esp_err_t initInputQueue(void);
static void otaButtonISR(void *params);
static void toggleButtonISR(void *params);

/**
 * Initializes the main input queue and all input buttons on the
 * board, creating the button tasks before enabling the button ISRs
 * that feed them.
 *
 * @note gpio_install_isr_service must be called before this function.
 *
 * @param otaTask A handle to the OTA task, which will be sent
 * task notifications when the OTA button is pressed.
 *
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if the main input queue is already initialized.
 * ESP_ERR_NO_MEM if the abort count semaphore could not be created.
 * ESP_FAIL if the main input queue could not be created.
 */
esp_err_t initInput(TaskHandle_t otaTask)
{
    esp_err_t err;

    if (NULL == otaTask) THROW_ERR(ESP_ERR_INVALID_ARG);

    err = initInputQueue();
    if (err != ESP_OK) THROW_ERR(err);

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
 * Initializes the input queue.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if the queue was already initialized.
 * ESP_FAIL if the queue could not be initialized.
 */
static esp_err_t initInputQueue(void)
{
    if (NULL != mainInputQueue) return ESP_ERR_INVALID_STATE;
    mainInputQueue = xQueueCreate(INPUT_QUEUE_LENGTH, sizeof(MainCommand));
    if (NULL == mainInputQueue) return ESP_FAIL;
    abortCountSemaphore = xSemaphoreCreateCounting(INPUT_QUEUE_LENGTH, 0);
    if (NULL == abortCountSemaphore) return ESP_ERR_NO_MEM;
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
