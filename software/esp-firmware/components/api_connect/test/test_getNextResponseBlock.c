/**
 * test_nextCSVEntryFromMark.c
 * 
 * Black box unit tests for api_connect.c:getNextResponseBlock.
 * 
 * Test file dependencies: 
 *     api_connect:test_openServerFile.c.
 *     common:test_circular_buffer.c.
 * 
 * Server file dependencies:
 *     CONFIG_TEST_DATA_BASE_URL/data_north_V1_0_5.csv.
 *     CONFIG_TEST_DATA_BASE_URL/getNextResponseBlock_typical.1.
 */

#include "api_connect_pi.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "sdkconfig.h"

#include "circular_buffer.h"

#define RETRY_NUM 5

extern esp_http_client_handle_t client;

/**
 * Tests input guards.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("getNextResponseBlock_inputGuards", "[api_connect]")
{
    const char *URL = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/data_north_V1_0_5.csv";
    esp_err_t err;
    int len;
    char buffer[10];
    int64_t contentLen;


    err = openServerFile(&contentLen, client, URL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    len = 10;
    err = getNextResponseBlock(NULL, &len, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = getNextResponseBlock(buffer, NULL, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    len = 0;
    err = getNextResponseBlock(buffer, &len, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    len = 1;
    err = getNextResponseBlock(buffer, &len, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    len = 2;
    err = getNextResponseBlock(buffer, &len, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    len = 10;
    err = getNextResponseBlock(buffer, &len, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests a typical use case file.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("getNextResponseBlock_typical", "[api_connect]")
{
    const char *URL = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/data_north_V1_0_5.csv";
    esp_err_t err;
    int len;
    char buffer[10];
    char *expected;
    int64_t contentLen;

    err = openServerFile(&contentLen, client, URL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    expected = "1,71\r\n2,"; // 10th char is null-terminator
    len = 10;
    err = getNextResponseBlock(buffer, &len, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(8, len); // function reserves 2 chars
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    expected = "71\r\n3,71"; // 10th char is null-terminator
    len = 10;
    err = getNextResponseBlock(buffer, &len, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(8, len); // function reserves two chars
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}