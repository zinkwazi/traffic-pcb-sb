/**
 * circular_buffer.c
 * 
 * Implements a circular buffer data structure.
 */

#include "circular_buffer.h"
#include "circular_buffer_pi.h" // contains static declarations

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"

#include "app_err.h"
#include "utilities.h"

#define TAG "circBuffer"

/**
 * @brief Initializes the circular buffer to use backing (of length len) as the
 * datastructure's underlying array.
 * 
 * @param[in] buf The circular buffer being initialized.
 * @param[in] backing The underlying array to use in the data structure.
 * @param[in] len The length of the backing array.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 */
esp_err_t circularBufferInit(CircularBuffer *buf, char* backing, uint32_t len) {
    if (buf == NULL) return ESP_ERR_INVALID_ARG;
    if (backing == NULL) return ESP_ERR_INVALID_ARG;
    if (len == 0) return ESP_ERR_INVALID_ARG;

    buf->backing = backing;
    buf->backingSize = len;
    buf->end = 0;
    buf->len = 0;
    buf->mark = UINT32_MAX;
    return ESP_OK;
}

/**
 * @brief Stores len elements from str into the buffer.
 * 
 * @param[in] buf The circular buffer to place elements into.
 * @param[in] str The location to retrieve elements from.
 * @param[in] len The number of elements to retrieve from str; must not be
 * greater than the size of the buffer.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * APP_ERR_UNINITIALIZED if circular buffer is uninitialized.
 * ESP_ERR_INVALID_SIZE if len is larger than the buffer size.  
 */
esp_err_t circularBufferStore(CircularBuffer *buf, char *str, uint32_t len) {
    uint64_t len64;
    uint32_t curr, endPrime;
    bool lostMark;

    /* input guards */
    if (buf == NULL) return ESP_ERR_INVALID_ARG;
    if (str == NULL) return ESP_ERR_INVALID_ARG;
    if (len == 0) return ESP_ERR_INVALID_ARG;
    if (buf->backing == NULL) return APP_ERR_UNINITIALIZED;
    if (len > buf->backingSize) return ESP_ERR_INVALID_SIZE;

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
            return APP_ERR_UNINITIALIZED;
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
        return APP_ERR_LOST_MARK;
    }
    return ESP_OK;
}

/**
 * @brief Helps circularBufferStoreFromClient, particularly to reduce boilerplate
 * and allow quick 'mocking' of esp_http_client_read when testing.
 */
static int readHttpClientHelper(esp_http_client_handle_t client, char *buffer, int len)
{
    
}

/**
 * @brief Reads at most maxLen bytes from the client using esp_http_client_read
 * and stores them in the circular buffer.
 * 
 * @param[in] buf The circular buffer to store read data in.
 * @param[in] client The client to read data from. This function does not
 * cleanup the client.
 * @param[in] maxLen The maximum number of bytes to read from the client. If 0,
 * then enough bytes will be read to fill the buffer up to the bookmark or 
 * until the client is empty.
 * 
 * @requires:
 * - client is initialized and esp_http_client_read can be called on it.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * APP_ERR_UNINITIALIZED if buf is uninitialized or illegally modified.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t circularBufferStoreFromClient(CircularBuffer *buf, esp_http_client_handle_t client, uint32_t maxLen)
{
    int bytesRead;
    esp_err_t err;
    uint32_t startReadNdx, endReadNdx; // start-inclusive, end-exclusive

    /* input guards */
    if (buf == NULL) return ESP_ERR_INVALID_ARG;
    if (client == NULL) return ESP_ERR_INVALID_ARG;
    if (buf->backing == NULL) return APP_ERR_UNINITIALIZED;
    if (maxLen > buf->backingSize) return ESP_ERR_INVALID_SIZE;

    /* handle zero size circbuf case */
    if (buf->len == 0)
    {
        bytesRead = readHttpClientHelper(client, buf->backing, maxLen);
        if (bytesRead < 0) THROW_ERR(ESP_FAIL); // error code
        if (bytesRead > maxLen) THROW_ERR(ESP_FAIL);
        return ESP_OK;
    }

    /* determine reading indices for back of buffer */
    startReadNdx = buf->end + 1;
    if (buf->mark != UINT32_MAX && buf->mark > buf->end)
    {
        endReadNdx = buf->mark;
    } else
    {
        endReadNdx = buf->backingSize - 1;
    }

    /* read into back end of buffer */
    if (maxLen != 0 && endReadNdx - startReadNdx >= maxLen)
    {
        /* will not need to read into front of buffer */
        bytesRead = readHttpClientHelper(client, &(buf->backing[startReadNdx]), maxLen);
        if (bytesRead < 0) THROW_ERR(ESP_FAIL);
        if (bytesRead > maxLen) THROW_ERR(ESP_FAIL);
        buf->len += bytesRead;
        if (buf->len > buf->backingSize) THROW_ERR(ESP_FAIL); // unexpected
        buf->end += bytesRead; // will not be OOB
        return ESP_OK;
    }

    if (startReadNdx < buf->backingSize) // potentially OOB if buf->end == buf->backingSize - 1
    {
        bytesRead = readHttpClientHelper(client, &(buf->backing[startReadNdx]), endReadNdx - startReadNdx);
        if (bytesRead < 0) THROW_ERR(ESP_FAIL);
        if (bytesRead > maxLen) THROW_ERR(ESP_FAIL);
        buf->len += bytesRead;
        buf->end = modularAddition(buf->end, bytesRead, buf->backingSize);
        if (bytesRead < endReadNdx - startReadNdx) return ESP_OK; // reached end of client
        maxLen -= bytesRead;
    }

    /* determine reading indices for front of buffer */
    if (buf->end != buf->backingSize - 1) THROW_ERR(ESP_FAIL); // expected this to be true
    endReadNdx = (buf->mark < buf->end) ? buf->mark : buf->end;
    if (endReadNdx >= maxLen) endReadNdx = maxLen; // won't fill buffer entirely

    /* read into front end of buffer */
    bytesRead = readHttpClientHelper(client, buf->backing, endReadNdx);
    if (bytesRead < 0) THROW_ERR(ESP_FAIL);
    if (bytesRead > endReadNdx) THROW_ERR(ESP_FAIL);
    buf->len += bytesRead;
    if (buf->len > buf->backingSize) THROW_ERR(ESP_FAIL); // unexpected
    buf->end += bytesRead; // will not be OOB
    return ESP_OK;
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
 * @returns ESP_OK if successful; mark is unchanged if not CIRC_OK.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * APP_ERR_UNINITIALIZED if buf is uninitialized or was illegally modified.
 * APP_ERR_LOST_MARK if dist is calculated from the previous mark, but no
 * mark is present in the buffer.
 * ESP_ERR_INVALID_SIZE if the bookmark would be outside the length of the
 * buffer.
 */
esp_err_t circularBufferMark(CircularBuffer *buf, uint32_t dist, enum CircDistanceSetting setting) {
    uint32_t ndx, prevMarkDist;
    
    /* input guards */
    if (buf == NULL) return ESP_ERR_INVALID_ARG;
    if (buf->backing == NULL) return APP_ERR_UNINITIALIZED;

    /* calculate bookmark position */
    ndx = UINT32_MAX;
    switch (setting) {
        case FROM_PREV_MARK:
            if (buf->mark == UINT32_MAX) {
                return APP_ERR_LOST_MARK;
            }
            /* determine if new mark is beyond most recent char */
            prevMarkDist = modularSubtraction(buf->end, buf->mark, buf->backingSize) - 1;
            if (dist > prevMarkDist) return ESP_ERR_INVALID_SIZE;
            /* (buf->mark + dist) (mod buf->backingSize) */
            ndx = modularAddition(buf->mark, dist, buf->backingSize);
            break;
        case FROM_RECENT_CHAR:
            if (dist >= buf->len) return ESP_ERR_INVALID_SIZE;
            /* (buf->end - dist - 1) (mod buf->backingSize) */
            ndx = modularSubtraction(buf->end, dist, buf->backingSize);
            ndx = modularSubtraction(ndx, 1, buf->backingSize);
            break;
        case FROM_OLDEST_CHAR:
            if (dist >= buf->len) return ESP_ERR_INVALID_SIZE;
            /* (buf->end - buf->len + dist) (mod buf->backingSize) */
            ndx = modularSubtraction(buf->end, buf->len, buf->backingSize);
            ndx = modularAddition(ndx, dist, buf->backingSize);
            break;
        default:
            return APP_ERR_UNINITIALIZED;
    }
    
    /* create bookmark */
    if (ndx == UINT32_MAX) return APP_ERR_UNINITIALIZED;
    buf->mark = ndx;
    return ESP_OK;
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
 * @returns number of chars read if successful. 
 * -ESP_ERR_INVALID_ARG if invalid argument.
 * -ESP_ERR_INVALID_SIZE if len is larger than the length of data in the buffer.
 * -APP_ERR_UNINITIALIZED if buf is uninitialized or was illegally modified.
 */
int circularBufferRead(const CircularBuffer *buf, char *strOut, uint32_t len) {
    uint32_t curr, start, ndx;
    
    /* input guards */
    if (buf == NULL) return -ESP_ERR_INVALID_ARG;
    if (strOut == NULL) return -ESP_ERR_INVALID_ARG;
    if (len == 0) return -ESP_ERR_INVALID_ARG;
    if (buf->backing == NULL) return -APP_ERR_UNINITIALIZED;
    if (len > buf->len) return -ESP_ERR_INVALID_SIZE;

    /* calculate starting position of data */
    start = modularSubtraction(buf->end, len, buf->backingSize);
    if (start == UINT32_MAX) return -APP_ERR_UNINITIALIZED;

    /* read data */
    for (curr = 0; curr < len; curr++) {
        ndx = modularAddition(start, curr, buf->backingSize);
        if (ndx == UINT32_MAX) return -APP_ERR_UNINITIALIZED;
        strOut[curr] = buf->backing[ndx];
    }

    /* append null-terminator */
    strOut[len] = '\0';
    return len;
}

/**
 * @brief Retrieves at most maxLen elements starting from the bookmark in the 
 * buffer and stores them in strOut. Appends a null-terminator at strOut[len].
 * 
 * @param[in] buf The circular buffer to retrieve elements from.
 * @param[out] strOut The location to write elements to, null-terminated.
 * @param[in] maxLen The maximum number of elements to retrieve from the buffer,
 * not including the appended null-terminator.
 * 
 * @returns number of chars read if successful.
 * -ESP_ERR_INVALID_ARG if invalid argument.
 * -APP_ERR_UNINITIALIZED if buf is uninitialized or was illegally modified.
 * -APP_ERR_LOST_MARK if the buffer contains no mark.
 */
int circularBufferReadFromMark(const CircularBuffer *buf, char *strOut, uint32_t maxLen) {
    uint32_t strOutNdx = 0;
    uint32_t bufNdx;

    /* input guards */
    if (buf == NULL) return -ESP_ERR_INVALID_ARG;
    if (strOut == NULL) return -ESP_ERR_INVALID_ARG;
    if (maxLen == 0) return -ESP_ERR_INVALID_ARG;
    if (buf->backing == NULL) return -APP_ERR_UNINITIALIZED;
    if (buf->mark == UINT32_MAX) return -APP_ERR_LOST_MARK;

    /* read data */
    bufNdx = buf->mark;
    while (strOutNdx < maxLen) {
        strOut[strOutNdx] = buf->backing[bufNdx];
        strOutNdx++;
        bufNdx = modularAddition(bufNdx, 1, buf->backingSize);
        if (bufNdx == UINT32_MAX) {
            return -APP_ERR_UNINITIALIZED;
        }
        if (bufNdx == buf->end) { // must be AFTER logic bc buf->mark may equal buf->end
            break; // reached end of data
        }
    }
    /* append null-terminator */
    strOut[strOutNdx] = '\0';
    return strOutNdx;
}

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
 * UINT32_MAX if N is 0; Note that UINT32_MAX is not a possible
 * return value in normal operation because N is capped by UINT32_MAX
 * and the modular arithmetic cannot result in values of N.
 */
STATIC_IF_NOT_TEST uint32_t modularSubtraction(uint32_t a, uint32_t b, uint32_t N) {
    int64_t val64;
    /* input guards */
    if (N == 0) UINT32_MAX;
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
STATIC_IF_NOT_TEST uint32_t modularAddition(uint32_t a, uint32_t b, uint32_t N) {
    int64_t val64;
    /* input guards */
    if (N == 0) UINT32_MAX;
    /* calculate value */
    val64 = ((int64_t) a) + ((int64_t) b); // careful with uint32_t
    val64 = val64 % ((int64_t) N); // result cannot be negative bc addition
    return (uint32_t) val64; // val64 fits in uint32_t by % backingSize
}