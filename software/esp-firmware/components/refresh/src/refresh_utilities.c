/**
 * refresh_utilities.c
 * 
 * Created On: 8/28/26
 * Author: Jaden Baptista
 * 
 * Utilities for the refresh task useful for abstracting dependencies.
 */

#include "refresh_utilities.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "led_matrix.h"
#include "main_types.h"

#include "refresh_config.h"

/**
 * Sets the color of the given LED.
 * 
 * @param ledNum The Kicad LED number of the LED to set the color of.
 * @param color The color to set the LED to.
 * @param setScaling Whether to set the LED to the default brightness or
 * leave it at the current brightness.
 */
esp_err_t setLEDColor(uint16_t ledNum, Color color, bool setToDefaultBrightness) {
    esp_err_t err;

    /* update color */
    for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
    {
        err = matSetColor(ledNum, color.red, color.green, color.blue);
        if (err == ESP_OK) break;
    }
    if (err != ESP_OK) return err;
    if (!setToDefaultBrightness) return ESP_OK;

    /* set scaling if requested */
    for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
    {
        err = matSetScaling(ledNum, DEFAULT_SCALE, DEFAULT_SCALE, DEFAULT_SCALE);
        if (err == ESP_OK) break;
    }
    if (err != ESP_OK) return ESP_FAIL;    
    return ESP_OK;
}

esp_err_t clearRange()
{
    
}
