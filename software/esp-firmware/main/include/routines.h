/**
 * routines.h
 */

#ifndef ROUTINES_H_
#define ROUTINES_H_

#include <stdbool.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

/**
 * @brief The input parameters to dirButtonISR, which gives the routine
 * pointers to the main task's objects.
 */
struct DirButtonISRParams {
  TaskHandle_t mainTask; /*!< A handle to the main task used to send a 
                              notification. */
  TickType_t *lastISR; /*!< The tick that the last button interrupt was serviced.
                           Used for button debouncing. */
  bool *toggle; /*!< Indicates to the main task that the LED direction should
                     change from North to South or vice versa. The bool should
                     remain in-scope for the duration of use of this struct. */
};

typedef struct DirButtonISRParams DirButtonISRParams;

/**
 * @brief The input parameters to refreshTimerCallback, which gives the callback
 * pointers to the main task's objects.
 */
struct RefreshTimerParams {
  TaskHandle_t mainTask; /*!< A handle to the main task used to send a 
                              notification. */
  bool *toggle; /*!< Indicates to the main task that the LED direction should
                     change from North to South or vice versa. The bool should
                     remain in-scope for the duration of use of this struct. */
};

typedef struct RefreshTimerParams RefreshTimerParams;

esp_err_t initDirectionButton(bool *toggle);
esp_err_t enableDirectionButtonIntr(void);
esp_err_t disableDirectionButtonIntr(void);
esp_err_t initIOButton(TaskHandle_t otaTask);
esp_timer_handle_t createRefreshTimer(TaskHandle_t mainTask, bool *toggle);
esp_timer_handle_t createDirectionFlashTimer(void);

#endif /* ROUTINES_H_ */