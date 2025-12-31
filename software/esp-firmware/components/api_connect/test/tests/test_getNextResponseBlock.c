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
 *     CONFIG_API_CONN_TEST_DATA_BASE_URL/data_north_V1_0_5.csv.
 *     CONFIG_API_CONN_TEST_DATA_BASE_URL/getNextResponseBlock_typical.1.
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
#include "esp_mac.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "sdkconfig.h"

#include "circular_buffer.h"

#include "wrap_esp_http_client.h"
#include "mock_esp_http_client.h"
#include "resources/testApiConnectResources.h"

#define RETRY_NUM 5

extern esp_http_client_handle_t client;

/**
 * Tests input guards.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("getNextResponseBlock_inputGuards", "[api_connect]")
{
    DEFINE_DATA_NORTH_V1_0_5_ENDPOINT(data_north_V1_0_5);

    esp_err_t err;
    int len;
    char buffer[10];
    int64_t contentLen;

    /* define endpoints with expected query parameters */
    uint8_t mac[6];
    char query[MAX_QUERY_LEN];
    char endpointURL[MAX_URL_LEN + MAX_QUERY_LEN];

    err = esp_base_mac_addr_get(mac);
    snprintf(query, MAX_QUERY_LEN, "?id=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    strncpy(endpointURL, data_north_V1_0_5.url, MAX_URL_LEN);
    endpointURL[MAX_URL_LEN] = '\0';
    strncat(endpointURL, query, MAX_QUERY_LEN);

    const MockHttpEndpoint dataNorthV105EndpointWithParams = {
        .url = endpointURL,
        .responseCode = data_north_V1_0_5.responseCode,
        .response = data_north_V1_0_5.response,
        .contentLen = data_north_V1_0_5.contentLen,
    };

    err = mock_esp_http_client_add_endpoint(dataNorthV105EndpointWithParams);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* tests */
    err = openServerFile(&contentLen, client, data_north_V1_0_5.url, RETRY_NUM);
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

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests a typical use case file.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("getNextResponseBlock_typical", "[api_connect]")
{
    DEFINE_DATA_NORTH_V1_0_5_ENDPOINT(data_north_V1_0_5);
    
    esp_err_t err;
    int len;
    char buffer[10];
    char *expected;
    int64_t contentLen;

    /* define endpoints with expected query parameters */
    uint8_t mac[6];
    char query[MAX_QUERY_LEN];
    char endpointURL[MAX_URL_LEN + MAX_QUERY_LEN];

    err = esp_base_mac_addr_get(mac);
    snprintf(query, MAX_QUERY_LEN, "?id=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    strncpy(endpointURL, data_north_V1_0_5.url, MAX_URL_LEN);
    endpointURL[MAX_URL_LEN] = '\0';
    strncat(endpointURL, query, MAX_QUERY_LEN);

    const MockHttpEndpoint northV105EndpointWithParams = {
        .url = endpointURL,
        .responseCode = data_north_V1_0_5.responseCode,
        .response = data_north_V1_0_5.response,
        .contentLen = data_north_V1_0_5.contentLen,
    };

    err = mock_esp_http_client_add_endpoint(northV105EndpointWithParams);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* run test */
    err = openServerFile(&contentLen, client, data_north_V1_0_5.url, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    expected = "1,71\n2,7"; // 10th char is null-terminator
    len = 10;
    err = getNextResponseBlock(buffer, &len, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(8, len); // function reserves 2 chars
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    expected = "1\n3,71\n4"; // 10th char is null-terminator
    len = 10;
    err = getNextResponseBlock(buffer, &len, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(8, len); // function reserves two chars
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}