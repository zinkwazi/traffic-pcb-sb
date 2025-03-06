/**
 * app_errors.h
 * 
 * Contains functions for raising error states to the user.
 */

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

void throwNoConnError(ErrorResources *errRes, bool callerHasErrMutex);
void throwHandleableError(ErrorResources *errRes, bool callerHasErrMutex);
void throwFatalError(ErrorResources *errRes, bool callerHasErrMutex);
void resolveNoConnError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex);
void resolveHandleableError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex);

#endif /* ERROR_H_ */