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
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "led_matrix.h"

#include "pinout.h"


#define TAG "app_error"

#define ERROR_COLOR_RED (0xFF)
#define ERROR_COLOR_GREEN (0x00)
#define ERROR_COLOR_BLUE (0x00)

#define BACKTRACE_DEPTH (5)

static inline void indicateError(void);
static inline void indicateNoError(void);
static void startErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex);
static void stopErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex);
static void timerFlashErrCallback(void *params);

#if CONFIG_HARDWARE_VERSION == 1

/**
 * @brief Indicates to the user that an error is present.
 */
static inline void indicateError(void)
{
  /* intentionally ignoring error codes because 
     this is a best effort function */
  (void) gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
  (void) gpio_set_level(ERR_LED_PIN, 1);
}

/**
 * @brief Indicates to the user that no error is present.
 */
static inline void indicateNoError(void)
{
  /* intentionally ignoring error codes because 
     this is a best effort function */
  (void) gpio_set_level(ERR_LED_PIN, 0);
}
#elif CONFIG_HARDWARE_VERSION == 2

/**
 * @brief Indicates to the user that an error is present.
 */
static inline void indicateError(void) 
{
  /* intentionally ignoring error codes because 
     this is a best effort function */
  (void) matSetColor(ERROR_LED_NUM, ERROR_COLOR_RED, ERROR_COLOR_GREEN, ERROR_COLOR_BLUE);
}

/**
 * @brief Indicates to the user that no error is present.
 */
static inline void indicateNoError(void) 
{
  /* intentionally ignoring error codes because 
     this is a best effort function */
  (void) matSetColor(ERROR_LED_NUM, 0x00, 0x00, 0x00);
}

#else
#error "Unsupported hardware version!"
#endif

/**
 * @brief Moves the error state machine from the NO_ERR state to the
 *        NO_CONNECT state.
 * 
 * @note If a no connect error was already thrown, nothing happens.
 * @note If the state machine is in the HANDLEABLE_ERR state, then this function
 *       moves the FSM to the HANDLEABLE_AND_NO_SERVER_CONNECT_ERR state.
 * 
 * @param[in] errRes Global error state and task error synchronization 
 *        primitives.
 * @param[in] callerHasErrMutex Whether the caller already has possession of
 *        the error synchronization mutex, in which case the function will not
 *        attempt to take it.
 */
void throwNoConnError(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }
  
  switch (errRes->err) {
    case NO_ERR:
      errRes->err = NO_SERVER_CONNECT_ERR;
      /* falls through */
    case NO_SERVER_CONNECT_ERR:
      if (errRes->errTimer == NULL) {
        startErrorFlashing(errRes, true);
      }
      break;
    case HANDLEABLE_ERR:
      errRes->err = HANDLEABLE_AND_NO_SERVER_CONNECT_ERR;
      // do not flash because solid takes priority
      break;
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      break;
    case FATAL_ERR:
      throwFatalError(errRes, true);
      break;
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
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
void throwHandleableError(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  if (errRes->errTimer != NULL) {
    stopErrorFlashing(errRes, true);
  }
  indicateError();
  switch (errRes->err) {
    case NO_ERR:
      errRes->err = HANDLEABLE_ERR;
      break;
    case NO_SERVER_CONNECT_ERR:
      errRes->err = HANDLEABLE_AND_NO_SERVER_CONNECT_ERR;
      break;
    case HANDLEABLE_ERR:
      // cannot have multiple handleable errors at once
      /* falls through */
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      // cannot have multiple handleable errors at once
      ESP_LOGE(TAG, "multiple HANDLEABLE_ERR thrown!");
      /* falls through */
    case FATAL_ERR:
      throwFatalError(errRes, true);
      break;
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    ESP_LOGD(TAG, "releasing error semaphore");
    xSemaphoreGive(errRes->errMutex);
  }
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
void throwFatalError(ErrorResources *errRes, bool callerHasErrMutex) {
  ESP_LOGE(TAG, "FATAL_ERR thrown!");
  (void) esp_backtrace_print(BACKTRACE_DEPTH); // an error already occurred

  if (errRes == NULL) {
    indicateError();
    for (;;) {
      vTaskDelay(INT_MAX);
    }
  }

  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }
  if (errRes->errTimer != NULL) {
    stopErrorFlashing(errRes, true);
  }
  errRes->err = FATAL_ERR;
  indicateError();

#if CONFIG_FATAL_CAUSES_REBOOT == true
  vTaskDelay(pdMS_TO_TICKS(CONFIG_ERROR_PERIOD)); // let the error LED shine for a short time
  indicateError();
  esp_restart();
#endif /* CONFIG_FATAL_CAUSES_REBOOT == true */  

  xSemaphoreGive(errRes->errMutex); // give up mutex in caller's name
  /* calling task should not return */
  for (;;) {
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
void resolveNoConnError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  ESP_LOGW(TAG, "resolving NO_SERVER_CONNECT_ERR");
  if (errRes->errTimer != NULL) {
    stopErrorFlashing(errRes, true);
    indicateNoError();
  }
  switch (errRes->err) {
    case NO_SERVER_CONNECT_ERR:
      errRes->err = NO_ERR;
      break;
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      errRes->err = HANDLEABLE_ERR;
      break;
    case NO_ERR:
      /* falls through */
    case HANDLEABLE_ERR:
      if (resolveNone) {
        break;
      }
      ESP_LOGE(TAG, "resolving NO_SERVER_CONNECT_ERR without its error state");
      /* falls through */
    case FATAL_ERR:
      /* falls through */
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
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
void resolveHandleableError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  ESP_LOGW(TAG, "resolving HANDLEABLE_ERR");
  switch (errRes->err) {
    case HANDLEABLE_ERR:
      errRes->err = NO_ERR;
      indicateNoError();
      break;
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      errRes->err = NO_SERVER_CONNECT_ERR;
      if (errRes->errTimer == NULL) {
        startErrorFlashing(errRes, true);
      }
      break;
    case NO_ERR:
      /* falls through */
    case NO_SERVER_CONNECT_ERR:
      if (resolveNone) {
        break;
      }
      ESP_LOGE(TAG, "resolving HANDLEABLE_ERR without its error state");
      /* falls through */
    case FATAL_ERR:
      /* falls through */
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

/**
 * @brief Creates a periodic timer that toggles the error LED.
 * 
 * @note Sets a timer that calls timerFlashErrCallback. If a timer could not
 *       be started, then a fatal error is thrown.
 * 
 * @param timer A pointer to a timer handle that will point to the new timer
 *              if successful (ONLY if ESP_OK is returned).
 * @param ledStatus A pointer to an integer that will be used by the timer
 *                   callback. This should not be modified or destroyed until
 *                   the timer is no longer in use.
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources.
 */
static void startErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex) {
    const esp_timer_create_args_t timerArgs = {
      .callback = timerFlashErrCallback,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "errorTimer",
    };

    if (!callerHasErrMutex) {
      while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
    }
    if (esp_timer_create(&timerArgs, &(errRes->errTimer)) != ESP_OK) {
      throwFatalError(errRes, true);
    }
    if (esp_timer_start_periodic(errRes->errTimer, CONFIG_ERROR_PERIOD * 1000) != ESP_OK) {
      esp_timer_delete(errRes->errTimer);
      errRes->errTimer = NULL;
      throwFatalError(errRes, true);
    }

    if (!callerHasErrMutex) {
      xSemaphoreGive(errRes->errMutex);
    }
}

/**
 * @brief Stops the periodic timer that toggles the error LED.
 * 
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
static void stopErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  esp_timer_stop(errRes->errTimer);
  esp_timer_delete(errRes->errTimer);
  errRes->errTimer = NULL;

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
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