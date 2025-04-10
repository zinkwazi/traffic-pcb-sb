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
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"

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
esp_err_t strobeRegisterLED(uint16_t ledNum, 
                            uint8_t maxBrightness, 
                            uint8_t minBrightness,
                            uint8_t initBrightness, 
                            bool initStrobeUp)
{
    BaseType_t success;
    StrobeTaskCommand command;
    const QueueHandle_t strobeQueue = getStrobeQueue();

    if (strobeQueue == NULL) return ESP_ERR_INVALID_STATE;
    
    /* create command */
    command.caller = xTaskGetCurrentTaskHandle();
    command.ledNum = ledNum;
    command.maxScale = maxBrightness;
    command.minScale = minBrightness;
    command.initStrobeUp = initStrobeUp;
    command.registerLED = true;

    /* send command */
    do {
        success = xQueueSend(strobeQueue, &command, INT_MAX);
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

    if (strobeQueue == NULL) return ESP_ERR_INVALID_STATE;
    
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

    if (strobeQueue == NULL) return ESP_ERR_INVALID_STATE;
    
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