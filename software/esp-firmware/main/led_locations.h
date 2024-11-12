/**
 * led_locations.h
 * 
 * Contains the struct type 'LEDLoc' which defines a latitude/longitude 
 * associated with some LED. It also declares static constant arrays of 
 * all mappings between LED hardware number and LEDLoc, where each 
 * index = hardware number - 1.
 */

#ifndef LED_LOCATIONS_H_
#define LED_LOCATIONS_H_

#include "led_locations_arrs.h"
#include "tomtom.h"
#include <stdint.h>

static int southLEDLen = sizeof(southLEDLocs) / sizeof(southLEDLocs[0]);
static int northLEDLen = sizeof(northLEDLocs) / sizeof(northLEDLocs[0]);

/**
 * Gets the physical coordinates of the road segment
 * corresponding to the LED designated by ledNum, which
 * is the hardware number in Kicad of the LED.
 * 
 * Parameters:
 *  - ledNum: The location index number.
 *  - dir: The direction of travel of the road segment.
 * 
 * Returns: A pointer to the requested LEDLoc struct if
 *          successful, NULL if the index is out of bounds.
 */
const LEDLoc* getLoc(unsigned int locNdx, Direction dir);



#endif /* LED_LOCATIONS_H_ */

