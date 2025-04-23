/**
 * pinout.h
 * 
 * This file holds definitions for which ports and pins 
 * various data lines connect to on the PCB for V1_0.
 */

#ifndef V_PINOUT_H_
#define V_PINOUT_H_

#include "sdkconfig.h"

#if CONFIG_HARDWARE_VERSION == 2

#include "driver/gpio.h"

/* Status Indicators */
#define WIFI_LED_NUM     (414)
#define ERROR_LED_NUM    (413)
#define OTA_LED_NUM      (325)

/* LED Color Legend */
#define HEAVY_LED_NUM    (327)
#define MEDIUM_LED_NUM   (326)
#define LIGHT_LED_NUM    (328)

/* Settings Button */
#define IO_SW_PIN        GPIO_NUM_0 // Strapping pin, needs pullup

/* Direction Indicator */
#define T_SW_PIN         GPIO_NUM_48
#define NORTH_LED_NUM    (411)
#define SOUTH_LED_NUM    (409)
#define EAST_LED_NUM     (412)
#define WEST_LED_NUM     (410)

/* LED Matrix Pins */
#define INT1_PIN         GPIO_NUM_17
#define INT2_PIN         GPIO_NUM_15
#define INT3_PIN         GPIO_NUM_6
#define INT4_PIN         GPIO_NUM_11

/* I2C Pins */
#define I2C1_PORT        (-1) // auto-select available I2C port
#define SCL1_PIN         GPIO_NUM_9
#define SDA1_PIN         GPIO_NUM_10
#define I2C2_PORT        (-1) // auto-select available I2C port
#define SCL2_PIN         GPIO_NUM_18
#define SDA2_PIN         GPIO_NUM_8

/* Photoresistor pin */
#define PHOTO_PIN        GPIO_NUM_4
#define PHOTO_ADC_CHAN   (3)

/* USB Pins */
#define USB_MINUS        GPIO_NUM_19
#define USB_PLUS         GPIO_NUM_20

#endif /* CONFIG_HARDWARE_VERSION == 1 */
#endif /* V_PINOUT_H_ */