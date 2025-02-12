/**
 * dots_types.h
 */

#ifndef DOTS_TYPES_H_
#define DOTS_TYPES_H_

#include <stdint.h>

enum MatrixLocation {
    MAT1_PAGE0,
    MAT1_PAGE1,
    MAT2_PAGE0,
    MAT2_PAGE1,
    MAT3_PAGE0,
    MAT3_PAGE1,
#if CONFIG_HARDWARE_VERSION == 2
    MAT4_PAGE0,
    MAT4_PAGE1,
#endif
    MAT_NONE,
};

/**
 * @brief Contains the matrix registers that correspond to an LED.
 */
struct LEDReg {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    enum MatrixLocation matrix; // all LEDs have their registers on the same page
};

typedef struct LEDReg LEDReg;

#endif /* DOTS_TYPES_H_ */