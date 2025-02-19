/**
 * circular_buffer.c
 * 
 * Implements a circular buffer data structure.
 */

#include "circular_buffer.h"
#include "esp_err.h"
#include <stdbool.h>

#define TAG "circBuffer"

/**
 * @brief Initializes the circular buffer to use backing (of length len) as the
 *        datastructure's underlying array.
 * 
 * @param[in] buffer The circular buffer being initialized
 * @param[in] backing The underlying array to use in the data structure.
 * @param[in] len The length of the backing array.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t circularBufferInit(CircularBuffer *buf, char* backing, uint32_t len) {
    if (buf == NULL || backing == NULL || len == 0) {
        return ESP_FAIL;
    }

    buf->backing = backing;
    buf->backingSize = len;
    buf->end = 0;
    buf->len = 0;
    return ESP_OK;
}

/**
 * @brief Stores len elements from str into the buffer.
 * 
 * @param[in] buffer The circular buffer to place elements into.
 * @param[in] str The location to retrieve elements from.
 * @param[in] len The number of elements to retrieve from str.
 * 
 * @returns ESP_OK if successful. ESP_FAIL if len is larger than the size of the
 *          buffer, in which case it is assumed that a logic error in the caller
 *          has occurred. ESP_FAIL if an unexpected error occurred.
 */
esp_err_t circularBufferStore(CircularBuffer *buf, char *str, uint32_t len) {
    uint32_t curr;

    if (buf == NULL ||
        buf->backing == NULL ||
        buf->backingSize == 0 ||
        str == NULL ||
        len == 0 ||
        len > buf->backingSize) 
    {
        return ESP_FAIL;
    }

    /* store data from string */
    for (curr = 0; curr < len; curr++) {
        buf->backing[buf->end] = str[curr];
        buf->end = (buf->end + 1) % buf->backingSize;
    }
    /* update and clamp buffer length */
    buf->len += len;
    if (buf->len > buf->backingSize) {
        buf->len = buf->backingSize;
    }
    return ESP_OK;
}

/**
 * @brief Retrieves the most recent len elements in the buffer and stores them 
 *        in strOut. Appends a null-terminator at strOut[len].
 * 
 * @param[in] buffer The circular buffer to retrieve elements from.
 * @param[out] strOut The location to write elements to, null-terminated.
 * @param[in] len The number of elements to read from the buffer, not including
 *                the null-terminator.
 * 
 * @returns ESP_OK if successful. ESP_FAIL if len is larger than the size of
 *          the buffer or an unexpected error occurred.
 */
esp_err_t circularBufferRead(const CircularBuffer *buf, char *strOut, uint32_t len) {
    uint32_t curr, uStart;
    int64_t start64;
    uint64_t ndxBeforeMod;

    if (buf == NULL ||
        buf->backing == NULL ||
        buf->backingSize == 0 ||
        strOut == NULL ||
        len == 0 ||
        len > buf->len) 
    {
        return ESP_FAIL;
    }

    /* calculate starting position of data */
    start64 = ((int64_t) buf->end) - ((int64_t) len); // careful with uint32_t
    start64 = start64 % ((int64_t) buf->backingSize); // will be negative if start < 0
    if (start64 < 0) {
        // negative operand in % does not follow expected mod result
        start64 = buf->backingSize + start64;
    }
    uStart = (uint32_t) start64; // start64 fits in uint32_t by % backingSize
    /* read data */
    for (curr = 0; curr < len; curr++) {
        ndxBeforeMod = ((uint64_t) uStart) + ((uint64_t) curr); // uStart + curr fits in uint64_t
        
        strOut[curr] = buf->backing[ndxBeforeMod % ((uint64_t) buf->backingSize)];
    }
    /* append null-terminator */
    strOut[len] = '\0';
    return ESP_OK;
}