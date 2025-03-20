/**
 * test_nextCSVEntryFromMark.c
 * 
 * Black box unit tests for api_connect.c:nextCSVEntryFromMark.
 * 
 * Test file dependencies: common:test_circular_buffer.c
 */

#include "api_connect_pi.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity.h"
#include "esp_err.h"
#include "esp_log.h"

#include "circular_buffer.h"

#define TAG "test"

/**
 * Tests that ESP_ERR_NOT_FOUND is returned if no data was found.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("nextCSVEntryFromMark_noDataFound", "[api_connect]")
{
    esp_err_t err;
    circ_err_t circ_err;
    char *str = "456\r\n";
    const int TEST_BUF_SIZE = 9;
    const int CIRC_BUF_SIZE = 3 * TEST_BUF_SIZE;
    char buffer[TEST_BUF_SIZE];
    char circBufBacking[CIRC_BUF_SIZE];
    CircularBuffer circBuf;
    LEDData result;

    /* test uninitialized circular buffer */
    err = nextCSVEntryFromMark(&result, &circBuf, buffer, TEST_BUF_SIZE);
    TEST_ASSERT_EQUAL(CIRC_UNINITIALIZED, err);

    /* load string into circular buffer and mark it */
    circ_err = circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
    TEST_ASSERT_EQUAL(ESP_OK, circ_err);
    circ_err = circularBufferStore(&circBuf, str, strlen(str));
    TEST_ASSERT_EQUAL(ESP_OK, circ_err);
    circ_err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
    TEST_ASSERT_EQUAL(ESP_OK, circ_err);

    /* parse string through circular buffer */
    err = nextCSVEntryFromMark(&result, &circBuf, buffer, TEST_BUF_SIZE);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
}

/**
 * Tests input guards.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("nextCSVEntryFromMark_inputGuards", "[api_connect]")
{
    char *str = "4,71\r\n5";
    /* The maximum size of one test_data entry, including "\r\n" and '\0' */
    const int TEST_BUF_LEN = 9;
    const int CIRC_BUF_SIZE = 6 * TEST_BUF_LEN;
    char buffer[TEST_BUF_LEN];
    char circBufBacking[CIRC_BUF_SIZE];
    CircularBuffer circBuf;
    circ_err_t circ_err;
    esp_err_t err;
    int numBytes;
    LEDData result;
    char *expected;

    /* test NULL circular buffer */
    err = nextCSVEntryFromMark(&result, NULL, buffer, TEST_BUF_LEN);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    /* load first response block into circular buffer */
    circ_err = circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
    circ_err = circularBufferStore(&circBuf, str, strlen(str));
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
    circ_err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
    expected = "4,71\r\n5";
    numBytes = circularBufferReadFromMark(&circBuf, buffer, TEST_BUF_LEN - 1);
    TEST_ASSERT_EQUAL(strlen(expected), numBytes);
    TEST_ASSERT_EQUAL('\0', buffer[TEST_BUF_LEN - 1]);
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    /* test NULL response */
    err = nextCSVEntryFromMark(NULL, &circBuf, buffer, TEST_BUF_LEN);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    /* test NULL buffer */
    err = nextCSVEntryFromMark(&result, &circBuf, NULL, TEST_BUF_LEN);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    err = nextCSVEntryFromMark(&result, &circBuf, buffer, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/**
 * Tests that newline characters do not cause an infinite loop.
 * 
 * Test case dependencies: markMoved.
 */
TEST_CASE("nextCSVEntryFromMark_skipsNewline", "[api_connect]")
{
    char *str = "\n4,71\r\n5";
    /* The maximum size of one test_data entry, including "\r\n" and '\0' */
    const int TEST_BUF_LEN = 9;
    const int CIRC_BUF_SIZE = 6 * TEST_BUF_LEN;
    char buffer[TEST_BUF_LEN];
    char circBufBacking[CIRC_BUF_SIZE];
    CircularBuffer circBuf;
    circ_err_t circ_err;
    esp_err_t err;
    int numBytes;
    LEDData result;
    char *expected;

    /* load first response block into circular buffer */
    circ_err = circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
    circ_err = circularBufferStore(&circBuf, str, strlen(str));
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
    circ_err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
    expected = "\n4,71\r\n5";
    numBytes = circularBufferReadFromMark(&circBuf, buffer, TEST_BUF_LEN - 1);
    TEST_ASSERT_EQUAL(strlen(expected), numBytes);
    TEST_ASSERT_EQUAL('\0', buffer[TEST_BUF_LEN - 1]);
    TEST_ASSERT_EQUAL_STRING(expected, buffer);
    /* read LEDData from first response block */
    err = nextCSVEntryFromMark(&result, &circBuf, buffer, TEST_BUF_LEN);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(4, result.ledNum);
    TEST_ASSERT_EQUAL(71, result.speed);
    /* check that circular buffer mark was modified correctly */
    expected = "\n5";
    circ_err = circularBufferReadFromMark(&circBuf, buffer, TEST_BUF_LEN - 1);
    TEST_ASSERT_EQUAL_STRING(expected, buffer);
}

/**
 * Test case dependencies: None.
 */
TEST_CASE("nextCSVEntryFromMark_fullFile", "[api_connect]")
{
    extern const uint8_t test_data_start[] asm("_binary_data_north_V1_0_5_csv_start");
    extern const uint8_t test_data_end[] asm("_binary_data_north_V1_0_5_csv_end");

    extern const uint8_t test_expected_start[] asm("_binary_data_north_V1_0_3_dat_start");
    extern const uint8_t test_expected_end[] asm("_binary_data_north_V1_0_3_dat_end");

    /* The maximum size of one test_data entry, including two "\r\n" and one '\0' */
    const int TEST_BUF_LEN = 12;
    const int CIRC_BUF_SIZE = 2 * TEST_BUF_LEN;
    char buffer[TEST_BUF_LEN];
    char circBufBacking[CIRC_BUF_SIZE];
    CircularBuffer circBuf;
    circ_err_t circ_err;
    esp_err_t err;
    int numBytes;
    LEDData result;
    LEDData expectedLED;
    char *expected;
    int currLED, currDataNdx;

    /* load first response block into circular buffer */
    circ_err = circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
    circ_err = circularBufferStore(&circBuf, (char *) &test_data_start[0], TEST_BUF_LEN);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
    circ_err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
    expected = "1,71\r\n2,71\r";
    numBytes = circularBufferReadFromMark(&circBuf, buffer, TEST_BUF_LEN - 1);
    TEST_ASSERT_EQUAL(TEST_BUF_LEN - 1, numBytes);
    TEST_ASSERT_EQUAL('\0', buffer[TEST_BUF_LEN - 1]);
    TEST_ASSERT_EQUAL_STRING(expected, buffer);
    /* read LEDData from first response block */
    err = nextCSVEntryFromMark(&result, &circBuf, buffer, TEST_BUF_LEN);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(1, result.ledNum);
    TEST_ASSERT_EQUAL(71, result.speed);
    /* check that circular buffer mark was modified correctly */
    expected = "\n2,71\r\n";
    circ_err = circularBufferReadFromMark(&circBuf, buffer, TEST_BUF_LEN - 1);
    TEST_ASSERT_EQUAL_STRING(expected, buffer);
    /* store and retrieve the rest of the file */
    currLED = 2;
    currDataNdx = TEST_BUF_LEN;
    while (&test_data_start[currDataNdx] + TEST_BUF_LEN < test_data_end) {
        /* load next response block into circular buffer */
        circ_err = circularBufferStore(&circBuf, (char *) &test_data_start[currDataNdx], TEST_BUF_LEN);
        if (circ_err == CIRC_LOST_MARK) {
            ESP_LOGI(TAG, "lost mark after ledNum: %u", expectedLED.ledNum);
        }
        TEST_ASSERT_EQUAL(CIRC_OK, circ_err);
        /* read next entry from circular buffer */
        numBytes = circularBufferReadFromMark(&circBuf, buffer, TEST_BUF_LEN - 1);
        TEST_ASSERT_EQUAL(TEST_BUF_LEN - 1, numBytes);
        TEST_ASSERT_EQUAL('\0', buffer[TEST_BUF_LEN - 1]);
        /* read LEDData from buffer until there are no more full lines */
        err = nextCSVEntryFromMark(&result, &circBuf, buffer, TEST_BUF_LEN);
        /* allow ESP_OK, ESP_NOT_FOUND, and API_ERR_REMOVE_DATA */
        if (err == ESP_FAIL)
        {
            ESP_LOGI(TAG, "nextCSVEntryFromMark failed. Is there a -1 speed in the test data?");
        }
        TEST_ASSERT_NOT_EQUAL(ESP_FAIL, err);
        while (err != ESP_ERR_NOT_FOUND) {
            TEST_ASSERT_NOT_EQUAL(test_expected_end, &test_expected_start[currLED]);
            expectedLED.ledNum = (uint32_t) (currLED);
            expectedLED.speed = (uint32_t) test_expected_start[currLED]; // skip LED 0
            if (err != API_ERR_REMOVE_DATA &&
                (err != ESP_OK ||
                    expectedLED.ledNum != result.ledNum ||
                    expectedLED.speed != result.speed))
            {
                ESP_LOGI(TAG, "expected ledNum: %u", expectedLED.ledNum);
                ESP_LOGI(TAG, "expected speed: %d", expectedLED.speed);
            }
            if (err == API_ERR_REMOVE_DATA) {
                ESP_LOGI(TAG, "found remove data command");
                ESP_LOGI(TAG, "expected ledNum: %u", expectedLED.ledNum);
                ESP_LOGI(TAG, "result ledNum: %u", result.ledNum);
                ESP_LOGI(TAG, "result speed: %d", result.speed);
            }
            TEST_ASSERT_EQUAL(expectedLED.ledNum, result.ledNum);
            if (err == API_ERR_REMOVE_DATA) {
                TEST_ASSERT_EQUAL(UINT32_MAX, result.speed);
            } else {
                TEST_ASSERT_EQUAL(expectedLED.speed, result.speed);
            }
            currLED++;
            err = nextCSVEntryFromMark(&result, &circBuf, buffer, TEST_BUF_LEN);
            /* allow ESP_OK, ESP_NOT_FOUND, and API_ERR_REMOVE_DATA */
            TEST_ASSERT_NOT_EQUAL(ESP_FAIL, err);
        }
        currDataNdx += TEST_BUF_LEN;
    }
}