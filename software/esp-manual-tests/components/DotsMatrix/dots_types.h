/**
 * dots_types.h
 */

#ifndef DOTS_TYPES_H_
#define DOTS_TYPES_H_

#include <stdint.h>

/**
 * @brief Contains the matrix registers that correspond to an LED.
 */
struct LEDReg {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t page; // all LEDs have their registers on the same page
};

typedef struct LEDReg LEDReg;

#endif /* DOTS_TYPES_H_ */