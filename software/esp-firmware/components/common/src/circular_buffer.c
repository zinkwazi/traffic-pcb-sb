/**
 * circular_buffer.c
 * 
 * Implements a circular buffer data structure.
 */

#include "circular_buffer.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define TAG "circBuffer"

/**
 * @brief Calculates (a - b) (mod N).
 * 
 * @note C does not behave as expected with the naive % operator. This function
 * takes care of issues arising from negative values with the % operator and
 * from intermediate values not being representable by a uint32_t.
 * 
 * @param[in] a additive operand.
 * @param[in] b subtractive operand.
 * @param[in] N The modulus, which should not be 0.
 * 
 * @returns Result of (a - b) (mod N) if successful.
 *          UINT32_MAX if N is 0; Note that UINT32_MAX is not a possible
 *          return value in normal operation because N is capped by UINT32_MAX
 *          and the modular arithmetic cannot result in values of N.
 */
uint32_t modularSubtraction(uint32_t a, uint32_t b, uint32_t N) {
    int64_t val64;
    /* input guards */
    if (N == 0) {
        return UINT32_MAX;
    }
    /* calculate value */
    val64 = ((int64_t) a) - ((int64_t) b); // careful with uint32_t
    val64 = val64 % ((int64_t) N); // will be negative if val64 < 0
    if (val64 < 0) {
        // negative operand in % does not follow expected mod result
        val64 = N + val64;
    }
    return (uint32_t) val64; // val64 fits in uint32_t by % backingSize
}

/**
 * @brief Calculates (a + b) (mod N).
 * 
 * @note C does not behave as expected with the naive % operator. This function
 * takes care of issues arising from negative values with the % operator and
 * from intermediate values not being representable by a uint32_t.
 * 
 * @param[in] a additive operand.
 * @param[in] b additive operand.
 * @param[in] N The modulus, which should not be 0.
 * 
 * @returns Result of (a + b) (mod N) if successful.
 *          UINT32_MAX if N is 0; Note that UINT32_MAX is not a possible
 *          return value in normal operation because N is capped by UINT32_MAX
 *          and the modular arithmetic cannot result in values of N.
 */
uint32_t modularAddition(uint32_t a, uint32_t b, uint32_t N) {
    int64_t val64;
    /* input guards */
    if (N == 0) {
        return UINT32_MAX;
    }
    /* calculate value */
    val64 = ((int64_t) a) + ((int64_t) b); // careful with uint32_t
    val64 = val64 % ((int64_t) N); // result cannot be negative bc addition
    return (uint32_t) val64; // val64 fits in uint32_t by % backingSize
}

/**
 * @brief Initializes the circular buffer to use backing (of length len) as the
 *        datastructure's underlying array.
 * 
 * @param[in] buf The circular buffer being initialized.
 * @param[in] backing The underlying array to use in the data structure.
 * @param[in] len The length of the backing array.
 * 
 * @returns CIRC_OK if successful, otherwise CIRC_INVALID_ARG.
 */
circ_err_t circularBufferInit(CircularBuffer *buf, char* backing, uint32_t len) {
    if (buf == NULL || 
        backing == NULL || 
        len == 0) 
    {
        return CIRC_INVALID_ARG;
    }

    buf->backing = backing;
    buf->backingSize = len;
    buf->end = 0;
    buf->len = 0;
    buf->mark = UINT32_MAX;
    return CIRC_OK;
}

/**
 * @brief Stores len elements from str into the buffer.
 * 
 * @param[in] buf The circular buffer to place elements into.
 * @param[in] str The location to retrieve elements from.
 * @param[in] len The number of elements to retrieve from str; must not be
 *        greater than the size of the buffer.
 * 
 * @returns CIRC_OK if successful.
 *          CIRC_INVALID_ARG if invalid argument.
 *          CIRC_UNINITIALIZED if buf is uninitialized or was illegally modified.
 *          CIRC_INVALID_SIZE if len is larger than the size of the buffer.
 */
circ_err_t circularBufferStore(CircularBuffer *buf, char *str, uint32_t len) {
    uint64_t len64;
    uint32_t curr, endPrime;
    bool lostMark;
    /* input guards */
    if (buf == NULL ||
        str == NULL ||
        len == 0) 
    {
        return CIRC_INVALID_ARG;
    }
    if (buf->backing == NULL)
    {
        return CIRC_UNINITIALIZED;
    }
    if (len > buf->backingSize) {
        return CIRC_INVALID_SIZE;
    }
    /* check for bookmark destruction */
    lostMark = false;
    if (buf->mark != UINT32_MAX) {
        // endPrime denotes end if mark were shifted to index 0
        endPrime = modularSubtraction(buf->end, buf->mark, buf->backingSize);
        if (((uint64_t) endPrime) + ((uint64_t) len) - 1 >= ((uint64_t) buf->backingSize)) {
            // if mark is 0, then this is the only case when mark is destroyed,
            // because it denotes the case where writing will wrap around to index 0
            lostMark = true;
        }
    }
    /* store data from string */
    for (curr = 0; curr < len; curr++) {
        buf->backing[buf->end] = str[curr];
        buf->end = modularAddition(buf->end, 1, buf->backingSize);
        if (buf->end == UINT32_MAX) {
            return CIRC_UNINITIALIZED;
        }
    }
    /* update and clamp buffer length */
    len64 = ((uint64_t) buf->len) + ((uint64_t) len);
    if (len64 > ((uint64_t) buf->backingSize)) {
        buf->len = buf->backingSize;
    } else {
        buf->len = (uint32_t) len64;
    }
    /* remove bookmark if it is destroyed */
    if (lostMark) {
        buf->mark = UINT32_MAX; // denotes no bookmark
        return CIRC_LOST_MARK;
    }
    return CIRC_OK;
}

/**
 * @brief Creates a bookmark from which circularBufferReadFromMark can be
 *        called in relation to.
 * 
 * @note A circular buffer only supports one bookmark at a time and calling
 *       this function removes the previous mark.
 * 
 * @param[in] buf The circular buffer to create a mark in.
 * @param[in] dist The distance from either the previous mark or most recently
 *                 added char, determined by distSetting.
 * @param[in] setting Determines how dist is used in calculating the position
 *        of the bookmark.
 * 
 * @returns CIRC_OK if successful; mark is unchanged if not CIRC_OK.
 *          CIRC_INVALID_ARG if invalid argument.
 *          CIRC_UNINITIALIZED if buf is uninitialized or was illegally modified.
 *          CIRC_LOST_MARK if dist is calculated from the previous mark, but no
 *          mark is present in the buffer.
 *          CIRC_INVALID_SIZE if the bookmark would be outside the length of the
 *          buffer.
 */
circ_err_t circularBufferMark(CircularBuffer *buf, uint32_t dist, enum CircDistanceSetting setting) {
    uint32_t ndx, prevMarkDist;
    /* input guards */
    if (buf == NULL)
    {
        return CIRC_INVALID_ARG;
    }
    if (buf->backing == NULL) {
        return CIRC_UNINITIALIZED;
    }
    /* calculate bookmark position */
    ndx = UINT32_MAX;
    switch (setting) {
        case FROM_PREV_MARK:
            if (buf->mark == UINT32_MAX) {
                return CIRC_LOST_MARK;
            }
            /* determine if new mark is beyond most recent char */
            prevMarkDist = modularSubtraction(buf->end, buf->mark, buf->backingSize) - 1;
            if (dist > prevMarkDist) {
                return CIRC_INVALID_SIZE;
            }
            /* (buf->mark + dist) (mod buf->backingSize) */
            ndx = modularAddition(buf->mark, dist, buf->backingSize);
            break;
        case FROM_RECENT_CHAR:
            if (dist >= buf->len) {
                return CIRC_INVALID_SIZE;
            }
            /* (buf->end - dist - 1) (mod buf->backingSize) */
            ndx = modularSubtraction(buf->end, dist, buf->backingSize);
            ndx = modularSubtraction(ndx, 1, buf->backingSize);
            break;
        case FROM_OLDEST_CHAR:
            if (dist >= buf->len) {
                return CIRC_INVALID_SIZE;
            }
            /* (buf->end - buf->len + dist) (mod buf->backingSize) */
            ndx = modularSubtraction(buf->end, buf->len, buf->backingSize);
            ndx = modularAddition(ndx, dist, buf->backingSize);
            break;
        case DIST_SETTING_UNKNOWN:
            return CIRC_INVALID_ARG;
    }
    /* create bookmark */
    if (ndx == UINT32_MAX) {
        return CIRC_UNINITIALIZED;
    }
    buf->mark = ndx;
    return CIRC_OK;
}

/**
 * @brief Retrieves the most recent len elements in the buffer and stores them 
 *        in strOut. Appends a null-terminator at strOut[len].
 * 
 * @param[in] buf The circular buffer to retrieve elements from.
 * @param[out] strOut The location to write elements to, null-terminated.
 * @param[in] len The number of elements to read from the buffer, not including
 *        the null-terminator.
 * 
 * @returns CIRC_OK if successful. 
 *          CIRC_INVALID_ARG if invalid argument.
 *          CIRC_INVALID_SIZE if len is larger than the length of data in the buffer.
 *          CIRC_UNINITIALIZED if buf is uninitialized or was illegally modified.
 */
circ_err_t circularBufferRead(const CircularBuffer *buf, char *strOut, uint32_t len) {
    uint32_t curr, start, ndx;
    /* input guards */
    if (buf == NULL ||
        strOut == NULL ||
        len == 0)
    {
        return CIRC_INVALID_ARG;
    }
    if (buf->backing == NULL)
    {
        return CIRC_UNINITIALIZED;
    }
    if (len > buf->len) {
        return CIRC_INVALID_SIZE;
    }
    /* calculate starting position of data */
    start = modularSubtraction(buf->end, len, buf->backingSize);
    if (start == UINT32_MAX) {
        return CIRC_UNINITIALIZED;
    }
    /* read data */
    for (curr = 0; curr < len; curr++) {
        ndx = modularAddition(start, curr, buf->backingSize);
        if (ndx == UINT32_MAX) {
            return CIRC_UNINITIALIZED;
        }
        strOut[curr] = buf->backing[ndx];
    }
    /* append null-terminator */
    strOut[len] = '\0';
    return CIRC_OK;
}

/**
 * @brief Retrieves at most maxLen elements starting from the bookmark in the 
 *        buffer and stores them in strOut. Appends a null-terminator at strOut[len].
 * 
 * @param[in] buf The circular buffer to retrieve elements from.
 * @param[out] strOut The location to write elements to, null-terminated.
 * @param[in] maxLen The maximum number of elements to retrieve from the buffer,
 *        not including the appended null-terminator.
 * 
 * @returns CIRC_OK if successful.
 *          CIRC_INVALID_ARG if invalid argument.
 *          CIRC_UNINITIALIZED if buf is uninitialized or was illegally modified.
 *          CIRC_LOST_MARK if the buffer contains no mark.
 */
circ_err_t circularBufferReadFromMark(const CircularBuffer *buf, char *strOut, uint32_t maxLen) {
    uint32_t strOutNdx = 0;
    uint32_t bufNdx;
    /* input guards */
    if (buf == NULL ||
        strOut == NULL ||
        maxLen == 0)
    {
        return CIRC_INVALID_ARG;
    }
    if (buf->backing == NULL) {
        return CIRC_UNINITIALIZED;
    }
    if (buf->mark == UINT32_MAX) {
        return CIRC_LOST_MARK;
    }
    /* read data */
    bufNdx = buf->mark;
    while (strOutNdx < maxLen) {
        strOut[strOutNdx] = buf->backing[bufNdx];
        strOutNdx++;
        bufNdx = modularAddition(bufNdx, 1, buf->backingSize);
        if (bufNdx == UINT32_MAX) {
            return CIRC_UNINITIALIZED;
        }
        if (bufNdx == buf->end) { // must be AFTER logic bc buf->mark may equal buf->end
            break; // reached end of data
        }
    }
    /* append null-terminator */
    strOut[strOutNdx] = '\0';
    return CIRC_OK;
}