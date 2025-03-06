/**
 * dots_types.h
 */

#ifndef LED_TYPES_H_
#define LED_TYPES_H_

#include <stdint.h>

#if CONFIG_HARDWARE_VERSION == 1
enum MatrixLocation {
    MAT1_PAGE0,
    MAT1_PAGE1,
    MAT2_PAGE0,
    MAT2_PAGE1,
    MAT3_PAGE0,
    MAT3_PAGE1,
    MAT_NONE,
};
#elif CONFIG_HARDWARE_VERSION == 2
enum MatrixLocation {
    MAT1_PAGE0,
    MAT1_PAGE1,
    MAT2_PAGE0,
    MAT2_PAGE1,
    MAT3_PAGE0,
    MAT3_PAGE1,
    MAT4_PAGE0,
    MAT4_PAGE1,
    MAT_NONE,
};
#else
#error "Unsupported hardware version!"
#endif

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

#endif /* LED_TYPES_H_ */