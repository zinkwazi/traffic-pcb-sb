/**
 * routines.c
 */

#include "routines.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "app_err.h"
#include "led_matrix.h"
#include "pinout.h"

#define TAG "routines"

static void timerFlashDirCallback(void *params);
static void refreshTimerCallback(void *params);

/**
 * @brief Creates a timer that, when started, periodically sends task
 *        notifications to the main task to refresh the LEDs.
 * 
 * @param[in] mainTask The handle of the main task, used to send task 
 *        notifications.
 * @param[in] toggle A pointer to a portion of the main task state, which
 *        indicates to the main task that it should switch the current direction
 *        of the LEDs. This pointer should remain valid as long as the timer
 *        is in use.
 * 
 * @returns A handle to the created timer if successful, otherwise NULL.
 */
esp_timer_handle_t createRefreshTimer(TaskHandle_t mainTask, bool *toggle) {
  static RefreshTimerParams params;
  esp_timer_create_args_t timerArgs;
  esp_timer_handle_t ret;
  /* input guards */
  if (mainTask == NULL || toggle == NULL) {
    return NULL;
  }
  /* copy parameters */
  params.mainTask = mainTask;
  params.toggle = toggle;
  /* create timer */
  timerArgs.callback = refreshTimerCallback;
  timerArgs.arg = &params;
  timerArgs.dispatch_method = ESP_TIMER_TASK;
  timerArgs.name = "refreshTimer";
  if (esp_timer_create(&timerArgs, &ret) != ESP_OK) {
    return NULL;
  }
  return ret;
}

/**
 * @brief Creates a timer that periodically calls timerFlashDirCallback,
 *        which flashes the direction LEDs.
 * 
 * The first callback will cause the LEDs to light up, not turn off.
 * 
 * @returns A handle to the created timer if successful, otherwise NULL.
 */
esp_timer_handle_t createDirectionFlashTimer(void) {
  static int outputLevel;
  esp_timer_create_args_t timerArgs;
  esp_timer_handle_t ret;
  /* create timer */
  outputLevel = 0;
  timerArgs.callback = timerFlashDirCallback;
  timerArgs.arg = &outputLevel;
  timerArgs.dispatch_method = ESP_TIMER_TASK;
  timerArgs.name = "directionTimer";
  if (esp_timer_create(&timerArgs, &ret) != ESP_OK) {
    return NULL;
  }
  return ret;
}

/**
 * @brief Callback that periodically sends a task notification to the main task.
 * 
 * Callback that periodically tells the main task to refresh all LEDs if the 
 * direction button has not been pressed. The timer that calls this function 
 * restarts if the direction button is pressed.
 * 
 * @param params A TaskHandle_t that is the handle of the main task.
 */
static void refreshTimerCallback(void *params) {
  TaskHandle_t mainTask = ((RefreshTimerParams *) params)->mainTask;
  bool *toggle = ((RefreshTimerParams *) params)->toggle;

#ifdef CONFIG_TIMER_CAUSES_TOGGLE
  *toggle = true;
#endif

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(mainTask, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


#if CONFIG_HARDWARE_VERSION == 1
/**
 * @brief Callback that toggles all the direction LEDs.
 * 
 * Callback that is called from a timer that is active when the main task
 * requests a settings update from the user. This periodically toggles all
 * the direction LEDs, causing them to flash.
 * 
 * @param[in] params An int* used to store the current output value of the LEDs.
 *        This object should not be destroyed or modified while the timer using 
 *        this callback is active.
 */
static void timerFlashDirCallback(void *params) {
  int *currentOutput = (int *) params;
  *currentOutput = (*currentOutput == 1) ? 0 : 1;
  gpio_set_level(LED_NORTH_PIN, *currentOutput);
  gpio_set_level(LED_EAST_PIN, *currentOutput);
  gpio_set_level(LED_WEST_PIN, *currentOutput);
  gpio_set_level(LED_SOUTH_PIN, *currentOutput);
}
#elif CONFIG_HARDWARE_VERSION == 2
/**
 * @brief Callback that toggles all the direction LEDs.
 * 
 * Callback that is called from a timer that is active when the main task
 * requests a settings update from the user. This periodically toggles all
 * the direction LEDs, causing them to flash.
 * 
 * @param[in] params An int* used to store the current output value of the LEDs.
 *        This object should not be destroyed or modified while the timer using 
 *        this callback is active.
 */
static void timerFlashDirCallback(void *params) {
  int *currentOutput = (int *) params;
  *currentOutput = (*currentOutput == 1) ? 0 : 1;
  if (*currentOutput == 1)
  {
    (void) matSetColor(NORTH_LED_NUM, 0xFF, 0xFF, 0xFF);
    (void) matSetColor(EAST_LED_NUM, 0xFF, 0xFF, 0xFF);
    (void) matSetColor(WEST_LED_NUM, 0xFF, 0xFF, 0xFF);
    (void) matSetColor(SOUTH_LED_NUM, 0xFF, 0xFF, 0xFF);
  } else
  {
    (void) matSetColor(NORTH_LED_NUM, 0x00, 0x00, 0x00);
    (void) matSetColor(EAST_LED_NUM, 0x00, 0x00, 0x00);
    (void) matSetColor(WEST_LED_NUM, 0x00, 0x00, 0x00);
    (void) matSetColor(SOUTH_LED_NUM, 0x00, 0x00, 0x00);
  }
}
#else
#error "Unsupported hardware version!"
#endif