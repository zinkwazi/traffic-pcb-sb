/**
 * app_errors.c
 * 
 * Contains functions for raising error states to the user.
 */

#include "app_errors.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_debug_helpers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "led_matrix.h"
#include "pinout.h"

#include "app_err.h"

#define TAG "app_error"

#define ERROR_COLOR_RED (0xFF)
#define ERROR_COLOR_GREEN (0x00)
#define ERROR_COLOR_BLUE (0x00)

#define BACKTRACE_DEPTH (5)

/**
 * @brief Describes the combination of errors currently being handled.
 */
enum AppError {
    NO_ERR,
    NO_SERVER_CONNECT_ERR,
    HANDLEABLE_ERR,
    HANDLEABLE_AND_NO_SERVER_CONNECT_ERR,
    FATAL_ERR,
};

typedef enum AppError AppError;

/* Indicates the errors currently being handled by the application. This should
only be modified after errMutex is obtained.*/
static AppError sErrState = FATAL_ERR;

/* A timer that flashes the error LED if active. This should only be modified 
after errMutex is obtained. */
static esp_timer_handle_t sErrTimer = NULL;

/* A mutex that guards access to errState and errTimer */
static SemaphoreHandle_t sErrMutex = NULL;

static void startErrorFlashing(void);
static void stopErrorFlashing(void);
static void timerFlashErrCallback(void *params);
static inline void indicateError(void);
static inline void indicateNoError(void);

/**
 * @brief Initializes the app errors component, which allows use of app_errors.h
 * functions.
 * 
 * @requires:
 * - led_matrix component initialized.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if the component is already initialized.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t initAppErrors(void)
{
    /* check that component dependencies are initialized */
    if (getAppErrorsStatus() == ESP_OK) THROW_ERR((esp_err_t) ESP_ERR_INVALID_STATE);
    
    /* check that led_matrix component is already initialized */
    if (getLedMatrixStatus() != ESP_OK) THROW_ERR(ESP_FAIL);

    /* initialize static variables */
    sErrState = NO_ERR;
    sErrTimer = NULL;
    sErrMutex = xSemaphoreCreateMutex();
    if (sErrMutex == NULL) THROW_ERR(ESP_FAIL);

    return (esp_err_t) ESP_OK;
}

/**
 * @brief Returns whether the app_errors component is already initialized
 * via initAppErrors.
 * 
 * @returns ESP_OK if initialized, otherwise ESP_FAIL.
 */
esp_err_t getAppErrorsStatus(void)
{
    return (sErrMutex != NULL) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Moves the error state machine from the NO_ERR state to the
 *        NO_CONNECT state.
 * 
 * @note If a no connect error was already thrown, nothing happens.
 * @note If the state machine is in the HANDLEABLE_ERR state, then this function
 *       moves the FSM to the HANDLEABLE_AND_NO_SERVER_CONNECT_ERR state.
 * 
 * @requires:
 * - app_errors component initialized.
 * 
 * @param[in] errRes Global error state and task error synchronization 
 *        primitives.
 * @param[in] callerHasErrMutex Whether the caller already has possession of
 *        the error synchronization mutex, in which case the function will not
 *        attempt to take it.
 */
void throwNoConnError(void) {
    BaseType_t success;
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();
    /* input guards */
    if (getAppErrorsStatus() != ESP_OK) throwFatalError();

    /* acquire state mutex */
    if (xSemaphoreGetMutexHolder(sErrMutex) != caller)
    {
        success = xSemaphoreTake(sErrMutex, portMAX_DELAY);
        if (success != pdTRUE) throwFatalError();
    }

    /* follow state machine */
    switch (sErrState)
    {
        case NO_ERR:
            sErrState = NO_SERVER_CONNECT_ERR;
            /* falls through */
        case NO_SERVER_CONNECT_ERR:
            if (sErrTimer == NULL) startErrorFlashing();
            break;
        case HANDLEABLE_ERR:
            sErrState = HANDLEABLE_AND_NO_SERVER_CONNECT_ERR;
            // do not flash bc solid takes priority
            break;
        case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
            break;
        case FATAL_ERR:
            /* falls through */
        default:
            throwFatalError();
            break;
    }

    /* release state mutex */
    success = xSemaphoreGive(sErrMutex);
    if (success != pdTRUE) throwFatalError();
}

/**
 * @brief Moves the error state machine from the NO_ERR state to the
 *        HANDLEABLE_ERR state.
 * 
 * @note If a handleable error was already thrown without being resolved,
 *       then this function throws a fatal error.
 * @note If the state machine is in the NO_SERVER_CONNECT_ERR state, then
 *       this function moves the FSM to the HANDLEABLE_AND_NO_SERVER_CONNECT_ERR
 *       state.
 * 
 * @param[in] errRes Global error state and task error synchronization 
 *        primitives.
 * @param[in] callerHasErrMutex Whether the caller already has possession of
 *        the error synchronization mutex, in which case the function will not
 *        attempt to take it.
 */
void throwHandleableError(void) {
    BaseType_t success;
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();
    /* input guards */
    if (getAppErrorsStatus() != ESP_OK) throwFatalError();
  
    /* acquire state mutex */
    if (xSemaphoreGetMutexHolder(sErrMutex) != caller)
    {
        success = xSemaphoreTake(sErrMutex, portMAX_DELAY);
        if (success != pdTRUE) throwFatalError();
    }

    /* indicate error */
    if (sErrTimer != NULL)
    {
        stopErrorFlashing();
    }
    indicateError();

    /* modify state */
    switch (sErrState)
    {
        case NO_ERR:
            sErrState = HANDLEABLE_ERR;
            break;
        case NO_SERVER_CONNECT_ERR:
            sErrState = HANDLEABLE_AND_NO_SERVER_CONNECT_ERR;
            break;
        case HANDLEABLE_ERR:
            // cannot have multiple handleable errors at once
            /* falls through */
        case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
            // cannot have mutliple handleable errors at once
            ESP_LOGW(TAG, "mutliple HANDLEABLE_ERR thrown!");
            /* falls through */
        case FATAL_ERR:
            /* falls through */
        default:
            throwFatalError();
            break;
    }

    /* release state mutex */
    success = xSemaphoreGive(sErrMutex);
    if (success != pdTRUE) throwFatalError();
}

/**
 * @brief Indicates to the user a fatal error has occurred and traps
 *        the task in an infinite loop.
 * 
 * @note Not recoverable and overrides any recoverable errors.
 * 
 * @param[in] errRes Global error state and task error synchronization 
 *        primitives.
 * @param[in] callerHasErrMutex Whether the caller already has possession of
 *        the error synchronization mutex, in which case the function will not
 *        attempt to take it.
 */
void throwFatalError(void) {
    BaseType_t success;
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();

    /* input guards */
    ESP_LOGW(TAG, "FATAL_ERR thrown!");
    esp_backtrace_print(BACKTRACE_DEPTH);
    if (sErrMutex == NULL)
    {
        /* best effort to indicate fatal error */
        indicateError();
        for (;;) {
            vTaskDelay(INT_MAX);
        }
    }

    /* acquire error mutex */
    if (xSemaphoreGetMutexHolder(sErrMutex) != caller)
    {
        success = xSemaphoreTake(sErrMutex, portMAX_DELAY);
        if (success != pdTRUE)
        {
            /* best effort to indicate fatal error */
            indicateError();
            for (;;)
            {
                vTaskDelay(INT_MAX);
            }
        }
    }

    /* indicate fatal error */
    if (sErrTimer != NULL)
    {
        stopErrorFlashing();
    }
    sErrState = FATAL_ERR;
    indicateError();

#if CONFIG_FATAL_CAUSES_REBOOT == true
    vTaskDelay(pdMS_TO_TICKS(CONFIG_ERROR_PERIOD)); // let the error LED shine for a short time
    indicateError();
    esp_restart();
#endif /* CONFIG_FATAL_CAUSES_REBOOT == true */

    /* release error mutex */
    (void) xSemaphoreGive(sErrMutex); // best effort
    for (;;)
    {
        vTaskDelay(INT_MAX);
    }
}

/**
 * @brief Moves the error handler state machine out of the NO_SERVER_CONNECT_ERR
 *        state to the NO_ERR state and removes error indication.
 * 
 * @note If the FSM is in the HANDLEABLE_AND_NO_SERVER_CONNECT_ERR state, then
 *       this function moves the state to HANDLEABLE_ERR instead of NO_ERR.
 * 
 * @param[in] errResources A pointer to an ErrorResources holding global error
 *        handling resources.
 * @param[in] resolveNone Whether this function should complete if there is
 *        no handleable error present, or if it should throw a fatal error
 *        in that case.
 * @param[in] callerHasErrMutex Whether the caller already has possession of
 *        the error synchronization mutex, in which case the function will not
 *        attempt to take it.
 */
void resolveNoConnError(bool resolveNone) {
    BaseType_t success;
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();
    /* input guards */
    if (getAppErrorsStatus() != ESP_OK) throwFatalError();
  
    /* acquire state mutex */
    if (xSemaphoreGetMutexHolder(sErrMutex) != caller)
    {
        success = xSemaphoreTake(sErrMutex, portMAX_DELAY);
        if (success != pdTRUE) throwFatalError();
    }

    /* indicate no error, unless solid */
    ESP_LOGW(TAG, "resolving NO_SERVER_CONNECT_ERR");
    if (sErrTimer != NULL)
    {
        stopErrorFlashing();
        indicateNoError();
    }

    /* modify error state */
    switch (sErrState) {
        case NO_SERVER_CONNECT_ERR:
            sErrState = NO_ERR;
            break;
        case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
            sErrState = HANDLEABLE_ERR;
            break;
        case NO_ERR:
            /* falls through */
        case HANDLEABLE_ERR:
            if (resolveNone) break;
            ESP_LOGW(TAG, "resolving NO_SERVER_CONNECT_ERR without its error state");
            /* falls through */
        case FATAL_ERR:
            /* falls through */
        default:
            throwFatalError();
            break;
    }

    /* release error mutex */
    success = xSemaphoreGive(sErrMutex);
    if (success != pdTRUE) throwFatalError();
}

/**
 * @brief Moves the error handler state machine out of the HANDLEABLE_ERR state
 *        to the NO_ERR state and removes error indication.
 * 
 * @param[in] errResources A pointer to an ErrorResources holding global error
 *        handling resources.
 * @param[in] resolveNone Whether this function should complete if there is
 *        no handleable error present, or if it should throw a fatal error
 *        in that case.
 * @param[in] callerHasErrMutex Whether the caller already has possession of
 *        the error synchronization mutex, in which case the function will not
 *        attempt to take it.
 */
void resolveHandleableError(bool resolveNone) {
    BaseType_t success;
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();
    /* input guards */
    if (getAppErrorsStatus() != ESP_OK) throwFatalError();

    /* acquire state mutex */
    if (xSemaphoreGetMutexHolder(sErrMutex) != caller)
    {
        success = xSemaphoreTake(sErrMutex, portMAX_DELAY);
        if (success != pdTRUE) throwFatalError();
    }

    /* indicate no error, unless solid */
    ESP_LOGW(TAG, "resolving NO_SERVER_CONNECT_ERR");
    if (sErrTimer != NULL)
    {
        stopErrorFlashing();
        indicateNoError();
    }

    /* modify error state */
    switch (sErrState) {
        case HANDLEABLE_ERR:
            sErrState = NO_ERR;
            indicateNoError();
            break;
        case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
            sErrState = NO_SERVER_CONNECT_ERR;
            if (sErrTimer == NULL) startErrorFlashing();
            break;
        case NO_ERR:
            /* falls through */
        case NO_SERVER_CONNECT_ERR:
            if (resolveNone) break;
            ESP_LOGW(TAG, "resolving HANDLEABLE_ERR that doesn't exist");
            /* falls through */
        case FATAL_ERR:
            /* falls through */
        default:
            throwFatalError();
            break;
    }

    /* release error mutex */
    success = xSemaphoreGive(sErrMutex);
    if (success != pdTRUE) throwFatalError();
}

/**
 * @brief Creates a periodic timer that toggles the error LED.
 * 
 * @note Sets a timer that calls timerFlashErrCallback. If a timer could not
 *       be started, then a fatal error is thrown.
 * 
 * @requires:
 * - caller has sErrMutex.
 */
static void startErrorFlashing(void) {
    const esp_timer_create_args_t timerArgs = {
        .callback = timerFlashErrCallback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "errorTimer",
    };

    if (esp_timer_create(&timerArgs, &sErrTimer) != ESP_OK)
    {
        throwFatalError();
    }
    if (esp_timer_start_periodic(sErrTimer, CONFIG_ERROR_PERIOD * 1000) != ESP_OK)
    {
        esp_timer_delete(sErrTimer);
        sErrTimer = NULL;
        throwFatalError();
    }
}

/**
 * @brief Stops the periodic timer that toggles the error LED.
 * 
 * @requires:
 * - caller has sErrMutex.
 * 
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
static void stopErrorFlashing(void) {
  (void) esp_timer_stop(sErrTimer);
  (void) esp_timer_delete(sErrTimer);
  sErrTimer = NULL;
}

/**
 * @brief Callback that toggles the error LED.
 * 
 * Callback that is called from a timer that is active when the worker task
 * encounters an error. This periodically toggles the error LED, causing it
 * to flash. While this will take precedence over another error that causes
 * a solid high on the error LED, errors are synchronized by a semaphore. Thus
 * it should never be the case that two errors are indicated at once and the
 * LED will indicate the first type of error to occur.
 * 
 * @param params An int* used to store the current output value of the LED.
 *               This object should not be destroyed or modified while the
 *               timer using this callback is active.
 */
static void timerFlashErrCallback(void *params) {
    static bool currentOutput = false;
    currentOutput = (currentOutput) ? false : true;
    if (currentOutput)
    {
        indicateError();
    } else 
    {
        indicateNoError();
    }
}

/**
 * @brief Indicates to the user that an error is present.
 */
static inline void indicateError(void)
{
    /* intentionally ignoring error codes because 
    this is a best effort function */
#if CONFIG_HARDWARE_VERSION == 1
    (void) gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
    (void) gpio_set_level(ERR_LED_PIN, 1);
#elif CONFIG_HARDWARE_VERSION == 2
    (void) matSetColor(ERROR_LED_NUM, ERROR_COLOR_RED, ERROR_COLOR_GREEN, ERROR_COLOR_BLUE);
#else
#error "Unsupported hardware version!"
#endif
}

/**
 * @brief Indicates to the user that no error is present.
 */
static inline void indicateNoError(void)
{
    /* best effort function */
#if CONFIG_HARDWARE_VERSION == 1
    (void) gpio_set_level(ERR_LED_PIN, 0);
#elif CONFIG_HARDWARE_VERSION == 2
    (void) matSetColor(ERROR_LED_NUM, 0x00, 0x00, 0x00);
#else
#error "Unsupported hardware version!"
#endif
}