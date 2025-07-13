/**
 * routines.h
 */

#ifndef ROUTINES_H_
#define ROUTINES_H_

#include <stdbool.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

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

esp_timer_handle_t createRefreshTimer(TaskHandle_t mainTask, bool *toggle);
esp_timer_handle_t createDirectionFlashTimer(void);
esp_timer_handle_t createLoadingAnimTimer(void);

#endif /* ROUTINES_H_ */