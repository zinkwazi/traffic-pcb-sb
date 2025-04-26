/**
 * circular_buffer.h
 * 
 * Implements a circular buffer data structure.
 */

#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#include <stdint.h>

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
    /* a bookmark of an index of importance to the user, which is likely the 
       start of an unfinished line of data. A mark of UINT32_MAX is not 
       possible, thus it denotes that there is no mark */
    uint32_t mark;
};

typedef struct CircularBuffer CircularBuffer;

/* Denotes valid options for circularBufferMark. */
enum CircDistanceSetting {
    /* dist is from the previous mark to the 
    new mark in the positive direction. */
    FROM_PREV_MARK = 1,
    /* dist is from the most recently added char 
    to the new mark in the negative direction. */
    FROM_RECENT_CHAR = 2,
    /* dist is from the oldest char to the new
    mark in the positive direction. */
    FROM_OLDEST_CHAR = 3,
    /* unknown setting, ie. invalid argument */
    DIST_SETTING_UNKNOWN = 4,
};

typedef enum CircDistanceSetting CircDistanceSetting;

esp_err_t circularBufferInit(CircularBuffer *buffer, char* backing, uint32_t len);
esp_err_t circularBufferStore(CircularBuffer *buffer, char *str, uint32_t len);
esp_err_t circularBufferMark(CircularBuffer *buffer, uint32_t dist, CircDistanceSetting setting);
int circularBufferRead(const CircularBuffer *buffer, char *strOut, uint32_t len);
int circularBufferReadFromMark(const CircularBuffer *buffer, char *strOut, uint32_t len);

#endif /* CIRCULAR_BUFFER_H_ */