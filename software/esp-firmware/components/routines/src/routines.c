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
#include "input_queue.h"
#include "pinout.h"

#define TAG "routines"

static void timerFlashDirCallback(void *params);
static void refreshTimerCallback(void *params);

/**
 * @brief Creates a timer that sends a command to the main task when
 * it expires, which is useful for implementing a refresh timeout.
 * 
 * @returns A handle to the created timer if successful, otherwise NULL.
 */
esp_timer_handle_t createRefreshTimer(void) {
  esp_timer_handle_t ret;
  esp_timer_create_args_t timerArgs = {
    .name = "refreshTimer",
    .callback = refreshTimerCallback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
  };

  if (esp_timer_create(&timerArgs, &ret) != ESP_OK) return NULL;
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
 * @brief Callback that periodically pushes a command to the main queue.
 * 
 * @note If the queue is full, the timer's command is missed.
 * 
 * @note This function is not ISR safe. It should be called by the timer task.
 * 
 * @param params NULL
 */
static void refreshTimerCallback(void *params) {
  static const MainCommand cmd = MAIN_CMD_TIMEOUT;
  (void) xQueueSend(inputQueue, &cmd, 0); // best effort. timer is periodic, not oneshot
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