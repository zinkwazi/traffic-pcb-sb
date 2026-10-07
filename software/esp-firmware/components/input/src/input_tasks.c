/**
 * input_tasks.c
 *
 * Created On: 9/17/2026
 * Author: Jaden Baptista
 *
 * Contains input tasks that handle deferred processing
 * of button press IRQs to avoid increasing system jitter.
 */

#include "input_tasks.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "button_debounce_fsm.h"
#include "input_queue.h"
#include "pinout.h"

#define TAG "input_tasks"

#define BUTTON_EVENT_QUEUE_LEN (4)

typedef struct ToggleButtonTaskParams
{
    /* A timer that will expire if no IRQ is thrown after debouncing. */
    esp_timer_handle_t debounceTimer;
    /* A timer keeping track of long button presses. Separate to avoid noise resetting it. */
    esp_timer_handle_t longPressTimer;
    /* The FSM state of the button debounce logic. */
    ButtonDebounceState debounceFSM;
    /* The main program input queue. */
    QueueHandle_t mainInputQueue;
} ToggleButtonTaskParams;

typedef struct OTAButtonTaskParams
{
    /* A timer that will expire if no IRQ is thrown after debouncing. */
    esp_timer_handle_t debounceTimer;
    /* The FSM state of the button debounce logic. */
    ButtonDebounceState debounceFSM;
    /* The handle to the OTA task, for sending task notifications when the button is pressed. */
    TaskHandle_t otaTaskHandle;
} OTAButtonTaskParams;

/* Event queues of button/debounce timer events */
QueueHandle_t otaButtonEventQueue = NULL; // contains ButtonEvent
QueueHandle_t toggleButtonEventQueue = NULL; // contains ButtonEvent

/* Only one of each button task is ever created, so params live for as long
 * as the task does in static storage. This also avoids handing xTaskCreate
 * a pointer to a caller's stack frame. */
static OTAButtonTaskParams otaButtonTaskParams;
static ToggleButtonTaskParams toggleButtonTaskParams;
static TaskHandle_t otaButtonTaskHandle = NULL;
static TaskHandle_t toggleButtonTaskHandle = NULL;

static void otaButtonTask(void *params);
static void toggleButtonTask(void *params);
static void otaButtonDebounceTimerCallback(void *params);
static void toggleButtonDebounceTimerCallback(void *params);
static void toggleButtonLongPressTimerCallback(void *params);

/**
 * Creates the OTA button task, which handles deferred
 * processing of the OTA button ISR.
 *
 * @param[out] handle Where a handle to the created task
 * is created if successful. Can be NULL.
 * @param[in] prio The priority of the task.
 * @param[in] otaTaskHandle The task handle of the OTA task,
 * which this task will send task notifications to.
 *
 * @returns ESP_OK if the task was created successfully.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_NO_MEM if the event queue could not be created.
 * ESP_FAIL if failure.
 */
esp_err_t createOTAButtonTask(TaskHandle_t *handle, const UBaseType_t prio, TaskHandle_t otaTaskHandle)
{
    if (NULL == otaTaskHandle) return ESP_ERR_INVALID_ARG;

    otaButtonEventQueue = xQueueCreate(BUTTON_EVENT_QUEUE_LEN, sizeof(ButtonEvent));
    if (NULL == otaButtonEventQueue) return ESP_ERR_NO_MEM;

    const esp_timer_create_args_t debounceTimerArgs = {
        .name = "otaButtonDebounceTimer",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = otaButtonDebounceTimerCallback,
        .arg = NULL,
    };
    esp_err_t err = esp_timer_create(&debounceTimerArgs, &otaButtonTaskParams.debounceTimer);
    if (err != ESP_OK)
    {
        vQueueDelete(otaButtonEventQueue);
        otaButtonEventQueue = NULL;
        return err;
    }

    err = initButtonDebounceFSM(&otaButtonTaskParams.debounceFSM);
    if (err != ESP_OK)
    {
        (void) esp_timer_delete(otaButtonTaskParams.debounceTimer);
        vQueueDelete(otaButtonEventQueue);
        otaButtonEventQueue = NULL;
        return err;
    }

    otaButtonTaskParams.otaTaskHandle = otaTaskHandle;
    BaseType_t success = xTaskCreate(otaButtonTask, "otaButtonTask", 10000, &otaButtonTaskParams, prio,
                                      &otaButtonTaskHandle);
    if (success != pdPASS)
    {
        (void) esp_timer_delete(otaButtonTaskParams.debounceTimer);
        vQueueDelete(otaButtonEventQueue);
        otaButtonEventQueue = NULL;
        return ESP_FAIL;
    }

    /* the FSM starts out verifying the initial pin level, which settles
     * when the verification window elapses with no edge */
    (void) esp_timer_start_once(otaButtonTaskParams.debounceTimer, CONFIG_BUTTON_VERIFY_US); // best effort

    if (NULL != handle) *handle = otaButtonTaskHandle;
    return ESP_OK;
}

/**
 * Creates the toggle button task, which handles deferred
 * processing of the toggle button ISR.
 *
 * @param[out] handle Where a handle to the created task
 * is created if successful. Can be NULL.
 * @param[in] prio The priority of the task.
 * @param[in] mainInputQueue The main program input queue, which this
 * task will send commands to. Holds MainCommand type.
 *
 * @returns ESP_OK if the task was created successfully.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_NO_MEM if the event queue could not be created.
 * ESP_FAIL if failure.
 */
esp_err_t createToggleButtonTask(TaskHandle_t *handle, const UBaseType_t prio, QueueHandle_t mainInputQueue)
{
    if (NULL == mainInputQueue) return ESP_ERR_INVALID_ARG;

    toggleButtonEventQueue = xQueueCreate(BUTTON_EVENT_QUEUE_LEN, sizeof(ButtonEvent));
    if (NULL == toggleButtonEventQueue) return ESP_ERR_NO_MEM;

    const esp_timer_create_args_t debounceTimerArgs = {
        .name = "toggleButtonDebounceTimer",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = toggleButtonDebounceTimerCallback,
        .arg = NULL,
    };
    esp_err_t err = esp_timer_create(&debounceTimerArgs, &toggleButtonTaskParams.debounceTimer);
    if (err != ESP_OK)
    {
        vQueueDelete(toggleButtonEventQueue);
        toggleButtonEventQueue = NULL;
        return err;
    }

    const esp_timer_create_args_t longPressTimerArgs = {
        .name = "toggleButtonLongPressTimer",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = toggleButtonLongPressTimerCallback,
        .arg = NULL,
    };
    err = esp_timer_create(&longPressTimerArgs, &toggleButtonTaskParams.longPressTimer);
    if (err != ESP_OK)
    {
        (void) esp_timer_delete(toggleButtonTaskParams.debounceTimer);
        vQueueDelete(toggleButtonEventQueue);
        toggleButtonEventQueue = NULL;
        return err;
    }

    err = initButtonDebounceFSM(&toggleButtonTaskParams.debounceFSM);
    if (err != ESP_OK)
    {
        (void) esp_timer_delete(toggleButtonTaskParams.longPressTimer);
        (void) esp_timer_delete(toggleButtonTaskParams.debounceTimer);
        vQueueDelete(toggleButtonEventQueue);
        toggleButtonEventQueue = NULL;
        return err;
    }

    toggleButtonTaskParams.mainInputQueue = mainInputQueue;
    BaseType_t success = xTaskCreate(toggleButtonTask, "toggleButtonTask", 10000, &toggleButtonTaskParams, prio,
                                      &toggleButtonTaskHandle);
    if (success != pdPASS)
    {
        (void) esp_timer_delete(toggleButtonTaskParams.longPressTimer);
        (void) esp_timer_delete(toggleButtonTaskParams.debounceTimer);
        vQueueDelete(toggleButtonEventQueue);
        toggleButtonEventQueue = NULL;
        return ESP_FAIL;
    }

    /* see createOTAButtonTask */
    (void) esp_timer_start_once(toggleButtonTaskParams.debounceTimer, CONFIG_BUTTON_VERIFY_US); // best effort

    if (NULL != handle) *handle = toggleButtonTaskHandle;
    return ESP_OK;
}

/**
 * Expires when the OTA button's verification window elapses with no
 * further IRQ, posting a non-edge event so the FSM is re-run.
 *
 * @note Runs in the esp_timer task's context, not an ISR. Posting to the
 * queue (rather than touching otaButtonTaskParams.debounceFSM directly)
 * is what keeps this safe against the ISR: the queue serializes both
 * sources' events into one order, so only otaButtonTask ever reads or
 * writes the FSM state.
 */
static void otaButtonDebounceTimerCallback(void *params)
{
    const ButtonEvent event = {
        .edgeDetected = false,
        .pinHigh = (bool) gpio_get_level(IO_SW_PIN),
    };
    (void) xQueueSendToBack(otaButtonEventQueue, &event, 0); // best effort
}

/**
 * Expires when the toggle button's verification window elapses with no
 * further IRQ, posting a non-edge event so the FSM is re-run.
 *
 * @note Same safety note as otaButtonDebounceTimerCallback.
 */
static void toggleButtonDebounceTimerCallback(void *params)
{
    const ButtonEvent event = {
        .edgeDetected = false,
        .pinHigh = (bool) gpio_get_level(T_SW_PIN),
    };
    (void) xQueueSendToBack(toggleButtonEventQueue, &event, 0); // best effort
}

/**
 * Expires when the toggle button has been held past CONFIG_LONG_BUTTON_PRESS_US
 * without being released, registering a long (hold direction) press.
 */
static void toggleButtonLongPressTimerCallback(void *params)
{
    const MainCommand holdDirCommand = MAIN_CMD_HOLD_DIR_BTN;
    (void) xQueueSendToBack(toggleButtonTaskParams.mainInputQueue, &holdDirCommand, 0); // best effort
}

/**
 * The OTA button task, which handles deferred
 * processing of the OTA button ISR and debounce
 * timer callback.
 */
static void otaButtonTask(void *params)
{
    const esp_timer_handle_t debounceTimer = ((OTAButtonTaskParams *) params)->debounceTimer;
    ButtonDebounceState *const debounceFSM = &((OTAButtonTaskParams *) params)->debounceFSM;
    const TaskHandle_t otaTaskHandle = ((OTAButtonTaskParams *) params)->otaTaskHandle;
    ButtonEvent event;

    while (true)
    {
        /* wait for an edge (from the ISR) or a debounce timeout (from debounceTimer);
         * the queue preserves the order the two actually occurred in */
        (void) xQueueReceive(otaButtonEventQueue, &event, portMAX_DELAY);

        /* run processing */
        (void) esp_timer_stop(debounceTimer); // don't care about previous timer state
        ButtonDebounceFSMOutput out = buttonDebounceFSM(debounceFSM, event.pinHigh, event.edgeDetected);

        /* the button is pulled up, so a press is a high to low edge */
        if (BUTTON_EDGE_HIGH_TO_LOW == out.state)
        {
            (void) xTaskNotifyGive(otaTaskHandle); // cannot fail
        }

        if (out.monitorEdges)
        {
            gpio_intr_enable(IO_SW_PIN);
        } else
        {
            gpio_intr_disable(IO_SW_PIN);
        }
        if (0 != out.debounceTimeUs)
        {
            (void) esp_timer_start_once(debounceTimer, out.debounceTimeUs); // best effort
        }
    }

    ESP_LOGE(TAG, "OTA button task is exiting!");
    for (;;) {}
}

/**
 * The toggle button task, which handles deferred
 * processing of the toggle button ISR and debounce
 * timer callback.
 *
 * @param params ToggleButtonTaskParams *, which contains
 * params for the task.
 */
static void toggleButtonTask(void *params)
{
    const esp_timer_handle_t debounceTimer = ((ToggleButtonTaskParams *) params)->debounceTimer;
    const esp_timer_handle_t longPressTimer = ((ToggleButtonTaskParams *) params)->longPressTimer;
    ButtonDebounceState *const debounceFSM = &((ToggleButtonTaskParams *) params)->debounceFSM;
    const QueueHandle_t mainInputQueue = ((ToggleButtonTaskParams *) params)->mainInputQueue;
    const MainCommand quickCommand = MAIN_CMD_QUICK_DIR_BTN;
    ButtonEvent event;

    while (true)
    {
        /* wait for an edge (from the ISR) or a debounce timeout (from debounceTimer);
         * the queue preserves the order the two actually occurred in */
        (void) xQueueReceive(toggleButtonEventQueue, &event, portMAX_DELAY);

        /* run processing */
        (void) esp_timer_stop(debounceTimer); // don't care about previous timer state
        ButtonDebounceFSMOutput out = buttonDebounceFSM(debounceFSM, event.pinHigh, event.edgeDetected);

        if (BUTTON_EDGE_HIGH_TO_LOW == out.state)
        {
            /* the abort count must be incremented before the command is queued */
            (void) incrementAbortCount(); // best effort
            if (pdTRUE != xQueueSendToBack(mainInputQueue, &quickCommand, 0))
            {
                (void) decrementAbortCount(); // keep in sync with the dropped command
            }
            (void) esp_timer_start_once(longPressTimer, CONFIG_LONG_BUTTON_PRESS_US); // best effort
        } else if (BUTTON_EDGE_LOW_TO_HIGH == out.state)
        {
            (void) esp_timer_stop(longPressTimer); // don't care about previous timer state
        }

        if (out.monitorEdges)
        {
            gpio_intr_enable(T_SW_PIN);
        } else
        {
            gpio_intr_disable(T_SW_PIN);
        }
        if (0 != out.debounceTimeUs)
        {
            (void) esp_timer_start_once(debounceTimer, out.debounceTimeUs); // best effort
        }
    }

    ESP_LOGE(TAG, "Toggle button task is exiting!");
    for (;;) {}
}
