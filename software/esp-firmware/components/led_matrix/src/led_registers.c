/**
 * led_registers.c
 * 
 * Created On: 03/06/2026
 */

#include "led_registers.h"

#include <stdbool.h>

/**
 * @returns whether the LED is valid
 * and will work when 
 */
bool isLEDValid(LEDReg reg)
{
    if (reg.matrix >= MAT_NONE) return false;
    return true;
}