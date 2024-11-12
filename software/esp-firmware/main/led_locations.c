/**
 * led_locations.c
 * 
 * This file contains functions that support
 * translating Kicad led hardware numbers into
 * physical longitude/latitude pairs while avoiding
 * duplicating API requests.
 */

#include "led_locations.h"
#include "tomtom.h"
#include "freertos/FreeRTOS.h"
#include <stdint.h>

#define TAG "led_locations"

const LEDLoc* getLoc(unsigned int locNdx, Direction dir) {
    const LEDLoc *ledLocs = NULL;
    switch (dir) {
        case NORTH:
            ledLocs = northLEDLocs;
            if (locNdx >= northLEDLen) {
                return NULL;
            }
            break;
        case SOUTH:
            ledLocs = southLEDLocs;
            if (locNdx >= southLEDLen) {
                return NULL;
            }
            break;
        default:
            ESP_LOGE(TAG, "attempted to get location with unknown direction");
            return NULL;
    }
    return &(ledLocs[locNdx]);
}



