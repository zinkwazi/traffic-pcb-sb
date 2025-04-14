/**
 * strobe.c
 * 
 * Contains functionality for interacting with the strobe task.
 */

#include "strobe.h"
#include "strobe_task.h" // to access command queue
#include "strobe_types.h"

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "sdkconfig.h"

#include "app_err.h"

#define TAG "strobe"

#ifdef CONFIG_SUPPORT_STROBING

/**
 * @brief Pauses the strobe task from registering new LEDs from the strobe queue,
 * which allows tasks to add LEDs to the queue in chunks at a time. This removes
 * the potential for the chunk of LEDs to desync their strobing.
 * 
 * @note This affects strobe registration globally, meaning the queue should not
 * be paused for a long interval.
 * 
 * @requires:
 * - The caller must call resumeRegisteringLEDs when done registering a chunk
 * of LEDs that must have in-sync strobing.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL. If ESP_FAIL, another
 * task has likely paused the queue already, in which case an indefinite block
 * time with portMAX_DELAY.
 */
esp_err_t pauseStrobeRegisterLEDs(TickType_t blockTime)
{
    return acquireStrobeQueueMutex(blockTime);
}

/**
 * @brief Resumes the strobe task registering new LEDs from the strobe queue.
 * 
 * @requires:
 * - The caller must have previously called pauseRegisteringLEDs.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t resumeStrobeRegisterLEDs(void)
{
    return releaseStrobeQueueMutex();
}

/**
 * @brief Sends a command to the strobe task to register strobing of ledNum.
 * 
 * @note The strobe task will ignore this command if strobing is already
 *       installed on the target LED. This is a silent failure and may cause
 *       a subsequent call to strobeUnregisterLED to also be ignored.
 * 
 * @param[in] ledNum The LED to register strobing to.
 * @param[in] maxBrightness The maximum brightness to strobe the LED to.
 * @param[in] minBrightness The minimum brightness to strobe the LED to.
 * @param[in] initBrightness The initial brightness to start strobing from. If
 * this is greater than maxBrightness, the initial strobe direction will be
 * decreasing and the initial brightness will be maxBrightness and vice-versa
 * with minBrightness.
 * @param[in] initStrobeUp Whether strobing begins by increasing to the maximum
 * value or by decreasing to the minimum value.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_STATE if the command queue is not initialized.
 */
esp_err_t strobeRegisterLED(StrobeTaskCommand command)
{
    BaseType_t success;
    const QueueHandle_t strobeQueue = getStrobeQueue();

    if (strobeQueue == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    
    /* create command */
    command.caller = xTaskGetCurrentTaskHandle();
    command.registerLED = true;

    /* send command */
    do {
        success = xQueueSend(strobeQueue, &command, INT_MAX); // shallow copy of command
    } while (success != pdPASS);
    return ESP_OK;
}

/**
 * @brief Sends a command to the strobe task to unregister strobing of ledNum.
 * 
 * @note The strobe task will ignore this command if the calling task was not
 *       the original task to install strobing on the target LED.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_STATE if the command queue is not initialized.
 */
esp_err_t strobeUnregisterLED(uint16_t ledNum)
{
    BaseType_t success;
    StrobeTaskCommand command;
    const QueueHandle_t strobeQueue = getStrobeQueue();

    if (strobeQueue == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    
    /* create command */
    command.caller = xTaskGetCurrentTaskHandle();
    command.ledNum = ledNum;
    command.registerLED = false;

    /* send command */
    do {
        success = xQueueSend(strobeQueue, &command, INT_MAX);
    } while (success != pdPASS);
    return ESP_OK;
}

/**
 * @brief Sends a command to the strobe task to unregister strobing of all LEDs 
 *        that were registered by the calling task.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_STATE if the command queue is not initialized.
 */
esp_err_t strobeUnregisterAll(void)
{
    BaseType_t success;
    StrobeTaskCommand command;
    const QueueHandle_t strobeQueue = getStrobeQueue();

    if (strobeQueue == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    
    /* create command */
    command.caller = xTaskGetCurrentTaskHandle();
    command.ledNum = UINT16_MAX; // indicates unregister all
    command.registerLED = false;

    /* send command */
    do {
        success = xQueueSend(strobeQueue, &command, INT_MAX);
    } while (success != pdPASS);
    return ESP_OK;
}

#endif /* CONFIG_SUPPORT_STROBING */