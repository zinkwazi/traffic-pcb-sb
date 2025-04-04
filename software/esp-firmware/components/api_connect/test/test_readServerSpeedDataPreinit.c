/**
 * test_readServerSpeedDataPreinit.c
 * 
 * Black box unit tests for api_connect.c:readServerSpeedDataPreinit.
 * 
 * Test file dependencies:
 *     api_connect:test_nextCSVEntryFromMark.c
 *     api_connect:test_getNextResponseBlock.c
 *     common:test_circular_buffer.c
 * 
 * Server file dependencies:
 *     CONFIG_API_CONN_TEST_DATA_BASE_URL/data_north_V1_0_5.csv.
 *     CONFIG_API_CONN_TEST_DATA_BASE_URL/readServerSpeedDataPreinit_smallFile.csv.
 */

#include "api_connect_pi.h"

#include "esp_log.h"
#include "unity.h"

#define RETRY_NUM 5

#define TAG "test"

extern esp_http_client_handle_t client;

/**
 * Tests input guards.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("readServerSpeedDataPreinit_inputGuards", "[api_connect]")
{
    esp_err_t err;
    CircularBuffer circBuf;
    uint32_t ledSpeedsLen = 5;
    LEDData ledSpeeds[ledSpeedsLen];

    err = readServerSpeedDataPreinit(NULL, ledSpeeds, ledSpeedsLen, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = readServerSpeedDataPreinit(&circBuf, NULL, ledSpeedsLen, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = readServerSpeedDataPreinit(&circBuf, ledSpeeds, 0, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = readServerSpeedDataPreinit(&circBuf, ledSpeeds, ledSpeedsLen, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/**
 * Tests a typical use case.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("readServerSpeedDataPreinit_smallFile", "[api_connect]")
{
    const char *URL = CONFIG_API_CONN_TEST_DATA_SERVER CONFIG_API_CONN_TEST_DATA_BASE_URL "/readServerSpeedDataPreinit_smallFile.csv";
    esp_err_t err;
    circ_err_t circ_err;
    CircularBuffer circBuf;
    uint32_t ledSpeedsLen = 5;
    LEDData ledSpeeds[ledSpeedsLen];
    int64_t contentLength;
    int outputLength;
    char circBufBacking[CIRC_BUF_SIZE];
    char buffer[RESPONSE_BLOCK_SIZE];

    err = openServerFile(&contentLength, client, URL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    outputLength = RESPONSE_BLOCK_SIZE;
    err = getNextResponseBlock(buffer, &outputLength, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    circ_err = circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);

    circ_err = circularBufferStore(&circBuf, buffer, (uint32_t) outputLength);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);

    circ_err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);

    err = readServerSpeedDataPreinit(&circBuf, ledSpeeds, ledSpeedsLen, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests a typical use case.
 * 
 * Test case dependencies: smallFile.
 */
TEST_CASE("readServerSpeedDataPreinit_typical", "[api_connect]")
{
    const char *URL = CONFIG_API_CONN_TEST_DATA_SERVER CONFIG_API_CONN_TEST_DATA_BASE_URL "/data_north_V1_0_5.csv";
    esp_err_t err;
    circ_err_t circ_err;
    CircularBuffer circBuf;
    uint32_t ledSpeedsLen = 326;
    int64_t contentLength;
    int outputLength;
    char circBufBacking[CIRC_BUF_SIZE];
    char buffer[RESPONSE_BLOCK_SIZE];
    LEDData ledSpeeds[ledSpeedsLen];

    err = openServerFile(&contentLength, client, URL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    outputLength = RESPONSE_BLOCK_SIZE;
    err = getNextResponseBlock(buffer, &outputLength, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    circ_err = circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);

    circ_err = circularBufferStore(&circBuf, buffer, (uint32_t) outputLength);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);

    circ_err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);

    err = readServerSpeedDataPreinit(&circBuf, ledSpeeds, ledSpeedsLen, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that a large file does not corrupt the stack for a small output array.
 * 
 * Test case dependencies: typical.
 */
TEST_CASE("readServerSpeedDataPreinit_memoryCorruption", "[api_connect]")
{
    const char *URL = CONFIG_API_CONN_TEST_DATA_SERVER CONFIG_API_CONN_TEST_DATA_BASE_URL "/data_north_V1_0_5.csv";
    esp_err_t err;
    circ_err_t circ_err;
    CircularBuffer circBuf;
    uint32_t ledSpeedsLen = 5;
    int64_t contentLength;
    int outputLength;
    char circBufBacking[CIRC_BUF_SIZE];
    char buffer[RESPONSE_BLOCK_SIZE];
    LEDData ledSpeeds[ledSpeedsLen];

    err = openServerFile(&contentLength, client, URL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    outputLength = RESPONSE_BLOCK_SIZE;
    err = getNextResponseBlock(buffer, &outputLength, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    circ_err = circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);

    circ_err = circularBufferStore(&circBuf, buffer, (uint32_t) outputLength);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);

    circ_err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
    TEST_ASSERT_EQUAL(CIRC_OK, circ_err);

    err = readServerSpeedDataPreinit(&circBuf, ledSpeeds, ledSpeedsLen, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}