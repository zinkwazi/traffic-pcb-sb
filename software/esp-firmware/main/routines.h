/**
 * routines.h
 */

#ifndef ROUTINES_H_
#define ROUTINES_H_

#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "utilities.h"
#include "dots_commands.h"
#include "pinout.h"
#include "tasks.h"
#include "main_types.h"

/**
 * @brief The input parameters to dirButtonISR, which gives the routine
 * pointers to the main task's objects.
 */
struct dirButtonISRParams {
  TaskHandle_t mainTask; /*!< A handle to the main task used to send a 
                              notification. */
  bool *toggle; /*!< Indicates to the main task that the LED direction should
                     change from North to South or vice versa. The bool should
                     remain in-scope for the duration of use of this struct. */ 
};

/**
 * @brief Interrupt service routine that handles direction button presses.
 * 
 * Handles direction button presses once the main task is 
 * ready to refresh LEDs. A button press is only acted upon
 * once the main task has refreshed all LEDs because the ISR
 * sends a task notification to the main task, which the task
 * only checks once it has finished handling a previous press.
 * 
 * @param params A pointer to a struct dirButtonISRParams that
 *               contains references to the main task's objects.
 */
void dirButtonISR(void *params);

/**
 * @brief Interrupt service routine that handles OTA button presses.
 * 
 * Handles OTA button presses to tell the main task to trigger an over-the-air
 * firmware upgrade.
 * 
 * @param params A TaskHandle_t that is the handle of the main task.
 */
void otaButtonISR(void *params);

/**
 * @brief Callback that periodically sends a task notification to the main task.
 * 
 * Callback that periodically tells the main task to refresh all LEDs if the 
 * direction button has not been pressed. The timer that calls this function 
 * restarts if the direction button is pressed.
 * 
 * @param params A TaskHandle_t that is the handle of the main task.
 */
void timerCallback(void *params);

/**
 * @brief Callback that toggles all the direction LEDs.
 * 
 * Callback that is called from a timer that is active when the main task
 * requests a settings update from the user. This periodically toggles all
 * the direction LEDs, causing them to flash.
 * 
 * @param params An int* used to store the current output value of the LEDs.
 *               This object should not be destroyed or modified while the
 *               timer using this callback is active.
 */
void timerFlashDirCallback(void *params);

#endif /* ROUTINES_H_ */