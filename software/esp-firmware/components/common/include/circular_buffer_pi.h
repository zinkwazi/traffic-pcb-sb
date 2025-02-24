/**
 * circular_buffer_pi.h
 * 
 * Contains a private interface of circular_buffer.c for white-box testing.
 */

#ifndef CIRCULAR_BUFFER_PI_H_
#define CIRCULAR_BUFFER_PI_H_
#ifndef CONFIG_DISABLE_TESTING_FEATURES

/* Include the public interface */
#include "circular_buffer.h"

/*******************************************/
/*            INTERNAL FUNCTIONS           */
/*******************************************/

uint32_t modularSubtraction(uint32_t a, uint32_t b, uint32_t N);
uint32_t modularAddition(uint32_t a, uint32_t b, uint32_t N);

#endif /* CONFIG_DISABLE_TESTING_FEATURES */
#endif /* CIRCULAR_BUFFER_PI_H_ */