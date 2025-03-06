/**
 * pinout.h
 * 
 * This file holds definitions for which ports and pins 
 * various data lines connect to on the PCB.
 */

#ifndef PINOUT_H_
#define PINOUT_H_

#include "sdkconfig.h"

#if CONFIG_HARDWARE_VERSION == 1
#include "V1_0_pinout.h"
#elif CONFIG_HARDWARE_VERSION == 2
#include "V2_0_pinout.h"
#else
#error "Unsupported hardware version!"
#endif

#endif /* PINOUT_H_ */