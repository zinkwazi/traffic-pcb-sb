/**
 * refresh_utilities.h
 * 
 * Created On: 8/28/26
 * Author: Jaden Baptista
 * 
 * Utilities for the refresh task useful for abstracting dependencies.
 */

#ifndef INC_REFRESH_UTILITIES_H_
#define INC_REFRESH_UTILITIES_H_

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "main_types.h"

#define SET_TO_DEFAULT_BRIGHTNESS       (true)
#define DONT_SET_BRIGHTNESS             (false)

esp_err_t setLEDColor(uint16_t ledNum, Color color, bool setToDefaultBrightness);

#endif /* INC_REFRESH_UTILITIES_H_ */