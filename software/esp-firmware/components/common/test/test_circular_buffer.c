/**
 * test_circular_buffer.c
 * 
 * Tests circular_buffer.h, which implements a simple circular buffer
 * data structure for chars.
 */

#include "circular_buffer_pi.h"
#include "unity.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string.h>
#include <stdint.h>

#define TAG "test"

/**
 * Test case dependencies: None.
 */
TEST_CASE("circularBufferInit", "[circular_buffer]")
{
    uint32_t backingLen = 20;
    CircularBuffer buffer;
    char backing[backingLen];
    
    circ_err_t err = circularBufferInit(&buffer, backing, backingLen);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL(backingLen, buffer.backingSize);
    TEST_ASSERT_EQUAL(0, buffer.len);
    TEST_ASSERT_EQUAL(UINT32_MAX, buffer.mark);
    TEST_ASSERT_EQUAL(backing, buffer.backing);

    err = circularBufferInit(&buffer, NULL, backingLen);
    TEST_ASSERT_EQUAL(CIRC_INVALID_ARG, err);

    err = circularBufferInit(&buffer, backing, 0);
    TEST_ASSERT_EQUAL(CIRC_INVALID_ARG, err);

    err = circularBufferInit(NULL, backing, backingLen);
    TEST_ASSERT_EQUAL(CIRC_INVALID_ARG, err);
}

/**
 * Defines the assumptions necessary for the circularBufferStore test case,
 * beyond dependencies of other test cases. 
 * 
 * These assumptions may change based on implementation; if this test case is 
 * not passing but the test case dependencies below are passing, then these 
 * assumptions are no longer valid and the circularBufferStore test case must 
 * be modified to reflect the changes.
 * 
 * Test case dependencies: circularBufferInit.
 */
TEST_CASE("assumptionsCircularBufferStore", "[circular_buffer]")
{
    uint32_t backingLen = 20;
    CircularBuffer buffer;
    char backing[backingLen];

    /* buffer.end is initialized to index 0 */
    esp_err_t err = circularBufferInit(&buffer, backing, backingLen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(0, buffer.end);
}

/**
 * Assumes AssumptionsCircularBufferStore passes.
 * 
 * Test case dependencies: circularBufferInit.
 */
TEST_CASE("circularBufferStore", "[circular_buffer]")
{
    const int TEST_MSG_LEN = 50;
    char message[TEST_MSG_LEN];
    memset(message, 0, TEST_MSG_LEN);
    uint32_t backingLen = 20;
    CircularBuffer buffer;
    char backing[backingLen];

    circ_err_t err = circularBufferInit(&buffer, backing, backingLen);
    TEST_ASSERT_EQUAL(CIRC_OK, err);

    /* test direct string, no overflow to ndx 0 */
    char *msg = "Hello, World!"; // len 13
    err = circularBufferStore(&buffer, msg, (uint32_t) strlen(msg));
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL((uint32_t) strlen(msg), buffer.end);
    TEST_ASSERT_EQUAL((uint32_t) strlen(msg), buffer.len);
    for (uint32_t i = 0; i < (uint32_t) strlen(msg); i++) {
        TEST_ASSERT_EQUAL(msg[i], backing[i]);
    }

    /* test negative buffer->end - buffer->len */
    char *msg2 = "second msg"; // len 10
    err = circularBufferStore(&buffer, msg2, (uint32_t) strlen(msg2));
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL(buffer.backingSize, buffer.len);
    TEST_ASSERT_EQUAL(3, buffer.end);
    char *expectedBacking = "msglo, World!second ";
    snprintf(message, TEST_MSG_LEN, "expected: \"%s\"", expectedBacking);
    snprintf(message, TEST_MSG_LEN, " backing: \"%s\"", backing);
    TEST_MESSAGE(message);
    for (uint32_t i = 0; i < backingLen; i++) {
        TEST_ASSERT_EQUAL(expectedBacking[i], backing[i]);
    }

    /* test input guards */
    err = circularBufferStore(&buffer, NULL, 10);
    TEST_ASSERT_EQUAL(CIRC_INVALID_ARG, err);
    err = circularBufferStore(&buffer, message, 50);
    TEST_ASSERT_EQUAL(CIRC_INVALID_SIZE, err); // function assumes this is a logic error
    err = circularBufferStore(NULL, msg, (uint32_t) strlen(msg));
    TEST_ASSERT_EQUAL(CIRC_INVALID_ARG, err);
    buffer.backing = NULL;
    err = circularBufferStore(&buffer, msg, (uint32_t) strlen(msg));
    TEST_ASSERT_EQUAL(CIRC_UNINITIALIZED, err);
}

/**
 * Test case dependencies: circularBufferInit, circularBufferWrite.
 */
TEST_CASE("circularBufferRead", "[circular_buffer]")
{
    const char CANARY_VAL = 't';
    const int TEST_MSG_LEN = 50;
    char message[TEST_MSG_LEN];
    memset(message, 0, TEST_MSG_LEN);
    uint32_t backingLen = 20;
    CircularBuffer buffer;
    char backing[backingLen];
    char canary1;
    char strOut[backingLen];
    char canary2;
    
    circ_err_t err = circularBufferInit(&buffer, backing, backingLen);
    TEST_ASSERT_EQUAL(CIRC_OK, err);

    /* test direct string, no overflow to ndx 0 */
    char *msg = "Hello, World!"; // len 13
    err = circularBufferStore(&buffer, msg, (uint32_t) strlen(msg));
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL((uint32_t) strlen(msg), buffer.len);
    err = circularBufferRead(&buffer, strOut, 13);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL_STRING(msg, strOut);

    /* test attempt to read more than is stored in buffer */
    uint32_t prevEnd = buffer.end;
    uint32_t prevLen = buffer.len;
    err = circularBufferRead(&buffer, strOut, 14);
    TEST_ASSERT_EQUAL(CIRC_INVALID_SIZE, err);
    TEST_ASSERT_EQUAL_STRING(msg, strOut);
    TEST_ASSERT_EQUAL(prevEnd, buffer.end);
    TEST_ASSERT_EQUAL(prevLen, buffer.len);

    /* test negative buffer->end - buffer->len */
    char *msg2 = "second msg"; // len 10
    err = circularBufferStore(&buffer, msg2, (uint32_t) strlen(msg2));
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL(buffer.backingSize, buffer.len);
    TEST_ASSERT_EQUAL(3, buffer.end);
    char *expected = "World!second msg";
    prevEnd = buffer.end;
    err = circularBufferRead(&buffer, strOut, strlen(msg2) + 6);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL(buffer.backingSize, buffer.len);
    TEST_ASSERT_EQUAL(prevEnd, buffer.end);
    TEST_ASSERT_EQUAL_STRING(expected, strOut);

    /* test input gaurds */
    prevEnd = buffer.end;
    prevLen = buffer.len;
    err = circularBufferRead(&buffer, NULL, 10);
    TEST_ASSERT_EQUAL(CIRC_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(prevEnd, buffer.end);
    TEST_ASSERT_EQUAL(prevLen, buffer.len);
    TEST_ASSERT_EQUAL_STRING(expected, strOut);

    err = circularBufferRead(NULL, strOut, 10);
    TEST_ASSERT_EQUAL(CIRC_INVALID_ARG, err);
    TEST_ASSERT_EQUAL_STRING(expected, strOut);

    prevEnd = buffer.end;
    prevLen = buffer.len;
    err = circularBufferRead(&buffer, strOut, 0);
    TEST_ASSERT_EQUAL(CIRC_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(prevEnd, buffer.end);
    TEST_ASSERT_EQUAL(prevLen, buffer.len);
    TEST_ASSERT_EQUAL_STRING(expected, strOut);

    /* test strOut buffer overflow */
    canary1 = CANARY_VAL;
    canary2 = CANARY_VAL;
    err = circularBufferRead(&buffer, strOut, backingLen);
    TEST_ASSERT_EQUAL(CANARY_VAL, canary1);
    TEST_ASSERT_EQUAL(CANARY_VAL, canary2);
}

/**
 * Also tests circularBufferReadFromMark.
 * 
 * Test case dependencies: circularBufferInit, circularBufferWrite.
 */
TEST_CASE("circularBufferMark", "[circular_buffer]")
{
    const int TEST_MSG_LEN = 50;
    char message[TEST_MSG_LEN];
    memset(message, 0, TEST_MSG_LEN);
    uint32_t backingLen = 20;
    CircularBuffer buffer;
    circ_err_t err;
    char backing[backingLen];
    char strOut[backingLen + 1];

    err = circularBufferInit(&buffer, backing, backingLen);
    TEST_ASSERT_EQUAL(CIRC_OK, err);

    /* write to buffer */
    char *msg = "Hello, World!"; // len 13
    err = circularBufferStore(&buffer, msg, (uint32_t) strlen(msg));
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL((uint32_t) strlen(msg), buffer.len);
    err = circularBufferRead(&buffer, strOut, 13);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL_STRING(msg, strOut);

    /* mark ndx 0 from recent char */
    err = circularBufferMark(&buffer, 12, FROM_RECENT_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL(0, buffer.mark);

    /* read from mark at ndx 0 */
    err = circularBufferReadFromMark(&buffer, strOut, 13);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL_STRING(msg, strOut);

    /* remark from recent mark */
    char *expected = "llo, World!";
    err = circularBufferMark(&buffer, 2, FROM_PREV_MARK);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL(2, buffer.mark);
    err = circularBufferReadFromMark(&buffer, strOut, backingLen);
    TEST_ASSERT_EQUAL_STRING(expected, strOut);

    /* mark beyond most recent char from prev mark */
    expected = "llo, World!";
    err = circularBufferMark(&buffer, 11, FROM_PREV_MARK);
    TEST_ASSERT_EQUAL(CIRC_INVALID_SIZE, err);
    err = circularBufferReadFromMark(&buffer, strOut, backingLen);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL_STRING(expected, strOut);

    /* mark oldest char */
    expected = "o, World!";
    err = circularBufferMark(&buffer, 4, FROM_OLDEST_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL(4, buffer.mark);
    err = circularBufferReadFromMark(&buffer, strOut, backingLen);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL_STRING(expected, strOut);

    /* mark beyond most recent char from oldest char */
    err = circularBufferMark(&buffer, 13, FROM_OLDEST_CHAR);
    TEST_ASSERT_EQUAL(CIRC_INVALID_SIZE, err);
    TEST_ASSERT_EQUAL(4, buffer.mark);

    /* mark from previous mark with no previous mark */
    buffer.mark = UINT32_MAX;
    err = circularBufferMark(&buffer, 2, FROM_PREV_MARK);
    TEST_ASSERT_EQUAL(CIRC_LOST_MARK, err);

    /* wrap buffer around */
    char *msg2 = "second msg";
    /* expected backing is 'msglo, World!second ' */
    expected = "ond msg";
    err = circularBufferStore(&buffer, msg2, strlen(msg2));
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    err = circularBufferMark(&buffer, 6, FROM_RECENT_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL(16, buffer.mark);
    err = circularBufferReadFromMark(&buffer, strOut, backingLen);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL_STRING(expected, strOut);

    /* mark beyond most recent char after wrap around */
    err = circularBufferMark(&buffer, 6, FROM_PREV_MARK);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL(2, buffer.mark);
    err = circularBufferReadFromMark(&buffer, strOut, backingLen);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    TEST_ASSERT_EQUAL_STRING("g", strOut);

    /* test input guards */
    err = circularBufferMark(NULL, 4, FROM_RECENT_CHAR);
    TEST_ASSERT_EQUAL(CIRC_INVALID_ARG, err);
    err = circularBufferMark(&buffer, 0, FROM_RECENT_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, err);
    buffer.backing = NULL;
    err = circularBufferMark(&buffer, 4, FROM_RECENT_CHAR);
    TEST_ASSERT_EQUAL(CIRC_UNINITIALIZED, err);
}  

/**
 * Tests the validity of the assumption that all non-zero
 * values of N for the modularAddition function never
 * result in UINT32_MAX.
 */
TEST_CASE("modularAddition", "[circular_buffer]")
{
    uint32_t out;
    
    out = modularAddition(UINT32_MAX, UINT32_MAX, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularAddition(UINT32_MAX, 0, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularAddition(0, 0, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularAddition(UINT32_MAX, 1, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularAddition(1, 1, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);

    out = modularAddition(UINT32_MAX, UINT32_MAX, 1);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularAddition(UINT32_MAX, 0, 1);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularAddition(0, 0, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularAddition(UINT32_MAX, 1, 1);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularAddition(1, 1, 1);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
}

/**
 * Tests the validity of the assumption that all non-zero
 * values of N for the modularSubtraction function never
 * result in UINT32_MAX.
 */
TEST_CASE("modularSubtraction", "[circular_buffer]")
{
    uint32_t out;
    
    out = modularSubtraction(UINT32_MAX, UINT32_MAX, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularSubtraction(UINT32_MAX, 0, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularSubtraction(0, 0, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularSubtraction(UINT32_MAX, 1, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularSubtraction(1, UINT32_MAX, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularSubtraction(1, 1, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);

    out = modularSubtraction(UINT32_MAX, UINT32_MAX, 1);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularSubtraction(UINT32_MAX, 0, 1);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularSubtraction(0, 0, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularSubtraction(UINT32_MAX, 1, 1);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
    out = modularSubtraction(1, 1, 1);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, out);
}