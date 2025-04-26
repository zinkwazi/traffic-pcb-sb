/**
 * circular_buffer_pi.h
 * 
 * Contains the private interface of circular_buffer.c for white-box testing.
 */

#ifndef CIRCULAR_BUFFER_PI_H_
#define CIRCULAR_BUFFER_PI_H_

/* Include the public interface */
#include "circular_buffer.h"

#include <stdint.h>

#include "utilities.h"

/*******************************************/
/*            INTERNAL FUNCTIONS           */
/*******************************************/

STATIC_IF_NOT_TEST uint32_t modularSubtraction(uint32_t a, uint32_t b, uint32_t N);
STATIC_IF_NOT_TEST uint32_t modularAddition(uint32_t a, uint32_t b, uint32_t N);

#endif /* CIRCULAR_BUFFER_PI_H_ */