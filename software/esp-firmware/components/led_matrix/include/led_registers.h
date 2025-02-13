/**
 * led_registers.h
 * 
 * This is a layer of indirection to particular led_register version
 * header files which includes the correct header for the version of
 * hardware the build is targeting.
 */

#ifndef LED_REGISTERS_H_
#define LED_REGISTERS_H_

#if CONFIG_HARDWARE_VERSION == 1
#include "V1_0_led_registers.h"
#endif

#if CONFIG_HARDWARE_VERSION == 2
#include "V2_0_led_registers.h"
#endif

/* new versions should explicitly specify which header file they need */

/** @brief The maximum number of LEDs that can be present on the device. */
#define MAX_NUM_LEDS sizeof(LEDNumToReg) / sizeof(LEDNumToReg[0])

#endif /* LED_REGISTERS_H_ */