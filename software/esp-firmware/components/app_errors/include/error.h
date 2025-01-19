

#ifndef ERROR_H_
#define ERROR_H_

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"



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

/**
 * @brief A group of pointers to resources necessary to synchronize errors
 *        that occur in various tasks.
 * 
 * @note These errors are presented to the user through the error LED, with
 *       each task being able to present an error to the user or crash the
 *       program. Some errors are recoverable; once a task has recovered it
 *       gives control of the error LED to another task that has encountered
 *       an error until all errors have been handled or the program restarts.
 */
struct ErrorResources {
    AppError err; /*!< Indicates the errors currently being handled by the 
                       application. This should only be modified after 
                       errMutex is obtained. */
    esp_timer_handle_t errTimer; /*!< A handle to a timer that flashes the
                                      error LED if active. This should be
                                      modified only after errMutex is 
                                      obtained. */
    SemaphoreHandle_t errMutex; /*!< A handle to a mutex that guards access
                                     to err. */
};

typedef struct ErrorResources ErrorResources;

/**
 * Expects errRes to not be NULL!
 */
void throwNoConnError(ErrorResources *errRes, bool callerHasErrMutex);

/**
 * Expects errRes to not be NULL!
 */
void throwHandleableError(ErrorResources *errRes, bool callerHasErrMutex);

/**
 * Expects errRes to not be NULL!
 */
void throwFatalError(ErrorResources *errRes, bool callerHasErrMutex);

/**
 * Expects errRes to not be NULL!
 */
void resolveNoConnError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex);

/**
 * Expects errRes to not be NULL!
 */
void resolveHandleableError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex);

/**
 * @brief Creates a periodic timer that toggles the error LED.
 * 
 * @note Sets a timer that calls timerFlashErrCallback. If a timer could not
 *       be started, then the error LED is set high.
 * 
 * @param timer A pointer to a timer handle that will point to the new timer
 *              if successful (ONLY if ESP_OK is returned).
 * @param ledStatus A pointer to an integer that will be used by the timer
 *                   callback. This should not be modified or destroyed until
 *                   the timer is no longer in use.
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources.
 */
void startErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex);

/**
 * @brief Stops the periodic timer that toggles the error LED.
 * 
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
void stopErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex);

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
void timerFlashErrCallback(void *params);

#endif /* ERROR_H_ */