/**
 * utilities.h
 * 
 * This file contains functions that may be useful to tasks
 * contained in various other header files.
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/**
 * Atomically tests and sets val to true. Returns whether
 * val was already true.
 */
bool boolWithTestSet(bool *val, SemaphoreHandle_t mutex);

#endif /* UTILITIES_H_ */