/**
 * pinout.h
 * 
 * This file holds definitions for which ports and pins 
 * various data lines connect to on the PCB.
 */

#ifndef PINOUT_H_
#define PINOUT_H_

/* IDF component includes */
#include "driver/gpio.h"

/* Status Indicators */
#define WIFI_LED_PIN    GPIO_NUM_19
#define ERR_LED_PIN     GPIO_NUM_21

/* Direction Indicator */
#define T_SW_PIN        GPIO_NUM_4
#define LED_NORTH_PIN   GPIO_NUM_18
#define LED_SOUTH_PIN   GPIO_NUM_17
#define LED_EAST_PIN    GPIO_NUM_5  // Strapping pin
#define LED_WEST_PIN    GPIO_NUM_16

/* LED Matrix Pins */
#define SBD1_PIN        GPIO_NUM_25
#define INT1_PIN        GPIO_NUM_35 // Input only
#define SBD2_PIN        GPIO_NUM_33
#define INT2_PIN        GPIO_NUM_34 // Input only
#define SBD3_PIN        GPIO_NUM_32
#define INT3_PIN        GPIO_NUM_39 // Input only, SENSOR_VN

/* I2C Pins */
#define I2C_PORT        (-1) // auto-select available I2C port
#define SCL_PIN         GPIO_NUM_26
#define SDA_PIN         GPIO_NUM_27

/* UART Pins */
#define RXD0_PIN        GPIO_NUM_3
#define TXD0_PIN        GPIO_NUM_1

#endif /* PINOUT_H_ */