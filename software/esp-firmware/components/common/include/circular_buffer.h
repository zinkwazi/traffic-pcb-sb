/**
 * circular_buffer.h
 * 
 * Implements a circular buffer data structure.
 */

#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#include "esp_err.h"

struct CircularBuffer {
    /* an array of characters used to implement the buffer */
    char *backing;
    /* the length of backing */
    uint32_t backingSize;
    /* the index of the last char added to the buffer */
    uint32_t end;
    /* the amount of data stored in the buffer, capped at backingSize */
    uint32_t len;
};

typedef struct CircularBuffer CircularBuffer;


esp_err_t circularBufferInit(CircularBuffer *buffer, char* backing, uint32_t len);


esp_err_t circularBufferStore(CircularBuffer *buffer, char *str, uint32_t len);


esp_err_t circularBufferRead(const CircularBuffer *buffer, char *strOut, uint32_t len);

#endif /* CIRCULAR_BUFFER_H_ */