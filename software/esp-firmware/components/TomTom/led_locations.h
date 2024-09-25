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

struct LEDLoc {
    double latitude;    // TODO: check whether these can be floats to save space
    double longitude;
};

typedef struct LEDLoc LEDLoc;

/* An array containing all southbound LED mappings */
static const LEDLoc southLEDLocs[] = {
    {34.420842, -119.702440}, // default point
    {34.456611, -119.976601}, // breaks http request??
    {34.457053, -119.976493},
    {34.441698, -119.949172},
};

/* An array containing all northbound LED mappings */
static const LEDLoc northLEDLocs[] = {
    {34.456611, -119.976601},
    {34.457053, -119.976493},
    {34.441698, -119.949172},
};

#endif /* LED_LOCATIONS_H_ */

