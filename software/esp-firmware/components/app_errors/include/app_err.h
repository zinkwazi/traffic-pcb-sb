/**
 * app_err.h
 * 
 * Contains error codes specific to failure conditions of various functions
 * across all components, which increases understandability of error sources.
 * For example, if a matrix function uses two functions that both return
 * ESP_FAIL, then two separate application error codes can be returned instead
 * of simply ESP_FAIL. This makes it possible for the application to recover
 * from errors originating from a specific source and eases debugging when
 * combined with stack traces from the THROW_ERR macro.
 * 
 * Note that, throughout the codebase, return paths for error codes are
 * denoted to be safe from collisions--in that they do not throw error codes 
 * that other return paths throw--by a non-functional cast to (esp_err_t).
 * These casts indicate only that if an error code is returned from a function,
 * that it is known to come from a particular source and nowhere else. This
 * may be important for runtime recover reasons, so be sure to check that
 * a new error code being thrown in a function does not collide with those
 * that are cast to (esp_err_t). Also make sure that changes to the errors
 * a function can throw does not interfere with calling function's 'safe' error
 * return paths. If so, make changes to avoid collisions in the caller.
 */

#ifndef APP_ERR_H_4_12_25
#define APP_ERR_H_4_12_25

#include <stdbool.h>

#include "esp_debug_helpers.h"
#include "esp_err.h"
#include "esp_log.h"

#define APP_ERR_BASE (0xe000) // does not collide with ESP_ERR_WIFI_BASE,
                              // ESP_ERR_MESH_BASE, ESP_ERR_FLASH_BASE,
                              // ESP_ERR_HW_CRYPTO_BASE, ESP_ERR_MEMPROT_BASE.

/* MatrixLocation enum value was invalid */
#define APP_ERR_INVALID_PAGE (APP_ERR_BASE + 1)
/* A failure to handle a mutex properly has occurred */
#define APP_ERR_MUTEX_FAIL (APP_ERR_BASE + 2)
/* A mutex timed out while being acquired */
#define APP_ERR_MUTEX_TIMEOUT (APP_ERR_BASE + 3)
/* A failure to release a mutex has occurred */
#define APP_ERR_MUTEX_RELEASE (APP_ERR_BASE + 4)
/* The error code was unhandled when defined handling is required */
#define APP_ERR_UNHANDLED (APP_ERR_BASE + 5)
/* The circular buffer bookmark would be/was destroyed */
#define APP_ERR_LOST_MARK (APP_ERR_BASE + 6)
/* An argument was uninitialized */
#define APP_ERR_UNINITIALIZED (APP_ERR_BASE + 7)

/* The backtrace depth to be printed during a bottom-level app error */
#define APP_ERR_BACKTRACE_DEPTH (5)

/**
 * @brief Returns app_err after printing the backtrace. This should only be
 * used when an error is being thrown, not being propogated through the stack.
 */
#define THROW_ERR(err)                                          \
    do {                                                        \
        ESP_LOGE(TAG, "err: 0x%x", err);                        \
        esp_backtrace_print(APP_ERR_BACKTRACE_DEPTH);           \
        return err;                                             \
    } while (0);

typedef struct {
    esp_err_t code;
} app_err_t;

#endif /* APP_ERR_H_4_12_25 */