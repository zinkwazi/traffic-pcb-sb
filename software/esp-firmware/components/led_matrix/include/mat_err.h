/**
 * mat_err.h
 * 
 * Contains error codes specific to the led_matrix component, which increases
 * understandability of error sources. For example, if a matrix function uses
 * two functions that both return ESP_FAIL, then two separate matrix error
 * codes should be returned instead of simply ESP_FAIL. This makes it possible
 * to recover from errors originating from a specific source.
 * 
 * When modifying the led_matrix component, make sure to be explicit about
 * checking whether an esp_err_t error code will interfere with a mat_err_t
 * code. This should be made explicit by casting an esp_err_t code to a
 * mat_err_t code, which implies that you as the programmer have checked that
 * the code is less than MAT_ERR_BASE.
 * 
 * Example: The example below is valid because ESP_FAIL < MAT_ERR_BASE.
 * 
 * if (success != pdTRUE) THROW_MAT_ERR((mat_err_t) ESP_FAIL)
 * 
 * Example: Casting an esp_err_t to mat_err_t is a statement that the programmer
 * has checked that the potential error codes from the function are less than
 * MAT_ERR_BASE.
 * 
 * mat_err = (mat_err_t) i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS);
 */

#ifndef MAT_ERR_H_4_8_25
#define MAT_ERR_H_4_8_25

#include "esp_err.h"
#include "esp_log.h"
#include "esp_debug_helpers.h"

/**
 * @brief A wrapper around esp_err_t indicating error codes should be
 * interpreted first as a matrix component error code and second as
 * a typical esp_err_t code, which increases error code understandability.
 * 
 * @note Wraps esp_err_t, meaning the two types can be cast interchangeably,
 * which reduces the overhead of using two error types. If the error code is 
 * less than MAT_ERR_BASE, then the code can safely be interpreted as a 
 * traditional esp_err_t code.
 */
typedef esp_err_t mat_err_t;

#define MAT_ERR_BASE (0x3000)



/* MatrixLocation enum value was invalid */
#define MAT_ERR_INVALID_PAGE (MAT_ERR_BASE + 1)
/* A timeout occurred taking a mutex */
#define MAT_ERR_MUTEX_TIMEOUT (MAT_ERR_BASE + 2)
/* A complete failure to handle the mutex properly has occurred */
#define MAT_ERR_MUTEX (MAT_ERR_BASE + 3)
/* The error code was unhandled when defined handling is required */
#define MAT_ERR_UNHANDLED (MAT_ERR_BASE + 4)

/* The backtrace depth to be printed during a bottom-level error */
#define MAT_ERROR_BACKTRACE (5)

/**
 * @brief Returns mat_err after printing the backtrace. This should only be
 * used when an error is being thrown, not being propogated through the stack.
 */
#define THROW_MAT_ERR(mat_err)                   \
    do {                                                     \
        ESP_LOGE(TAG, "Error! err: %d", mat_err);            \
        esp_backtrace_print(MAT_ERROR_BACKTRACE);            \
        return mat_err;                                      \
    } while (0);

#endif /* MAT_ERR_H_4_8_25 */