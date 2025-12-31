/**
 * test_openServerFile.c
 * 
 * Black box unit tests for api_connect.c:openServerFile.
 * 
 * Test file dependencies: None.
 * 
 * Server file dependencies: 
 *     CONFIG_API_CONN_TEST_DATA_BASE_URL/data_north_V1_0_5.csv.
 *     CONFIG_API_CONN_TEST_DATA_BASE_URL/openServerFile_typical.1.
 */

#include "api_connect_pi.h"

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "unity.h"

#include "wrap_esp_http_client.h"
#include "mock_esp_http_client.h"
#include "resources/testApiConnectResources.h"

#define RETRY_NUM 5

#define TAG "test"

extern esp_http_client_handle_t client;

/**
 * Tests input guards.
 * 
 * As the function might open a connection to the server, this test case
 * may break others that are dependent on opening another connection because
 * this test case does not close any connections that may open.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("openServerFile_inputGuards", "[api_connect]")
{
    DEFINE_DATA_NORTH_V1_0_5_ENDPOINT(data_north_V1_0_5);

    esp_err_t err;
    int64_t contentLength;

    err = mock_esp_http_client_add_endpoint(data_north_V1_0_5); // the endpoint should not be called
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(NULL, client, data_north_V1_0_5.url, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = openServerFile(&contentLength, NULL, data_north_V1_0_5.url, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = openServerFile(&contentLength, client, NULL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = openServerFile(&contentLength, client, data_north_V1_0_5.url, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = openServerFile(&contentLength, client, data_north_V1_0_5.url, -1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/**
 * Tests that a typical use case allows reading via esp_http_client_read.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("openServerFile_typical", "[api_connect]")
{
    DEFINE_OPEN_SERVER_FILE_TYPICAL_1_ENDPOINT(openServerFile_typical1);

    esp_err_t err;
    char typical1URL[MAX_URL_LEN + MAX_QUERY_LEN];
    uint8_t mac[6];
    const int32_t bufLen = 30;
    char buffer[bufLen];
    char *expected;
    int64_t contentLength = 0;

    /* define endpoints with expected query parameters */
    err = esp_base_mac_addr_get(mac);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    strncpy(typical1URL, openServerFile_typical1.url, MAX_URL_LEN);
    typical1URL[MAX_URL_LEN] = '\0';
    snprintf(buffer, MAX_QUERY_LEN, "?id=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    strncat(typical1URL, buffer, bufLen);

    const MockHttpEndpoint typical1EndpointWithParams = {
        .url = typical1URL,
        .responseCode = openServerFile_typical1.responseCode,
        .response = openServerFile_typical1.response,
        .contentLen = openServerFile_typical1.contentLen,
    };

    err = mock_esp_http_client_add_endpoint(typical1EndpointWithParams);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* run test */
    err = openServerFile(&contentLength, client, openServerFile_typical1.url, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(55, contentLength); // size of file

    expected = "abcdefghijklmnopqrstuvwxyz\nhe";
    do {
        contentLength = ESP_HTTP_CLIENT_READ(client, buffer, bufLen - 1);
    } while (contentLength == -ESP_ERR_HTTP_EAGAIN);
    TEST_ASSERT_EQUAL(bufLen - 1, contentLength);
    buffer[bufLen - 1] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that zero content length returns ESP_ERR_NOT_FOUND and closes client.
 *  
 * Test case dependencies: openServerFile_typical.
 */
TEST_CASE("openServerFile_zeroContentLength", "[api_connect]")
{
    DEFINE_OPEN_SERVER_FILE_TYPICAL_1_ENDPOINT(openServerFile_typical1);
    DEFINE_OPEN_SERVER_FILE_ZERO_LENGTH_1_ENDPOINT(openServerFile_zeroLength1);

    esp_err_t err;
    char typical1URL[MAX_URL_LEN + MAX_QUERY_LEN];
    char zeroLength1URL[MAX_URL_LEN + MAX_QUERY_LEN];
    uint8_t mac[6];
    const int32_t bufLen = 30;
    char buffer[bufLen];
    char *expected;
    int64_t contentLength;

    /* define endpoints with expected query parameters */
    err = esp_base_mac_addr_get(mac);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    snprintf(buffer, MAX_QUERY_LEN, "?id=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    strncpy(typical1URL, openServerFile_typical1.url, MAX_URL_LEN);
    typical1URL[MAX_URL_LEN] = '\0';
    strncat(typical1URL, buffer, MAX_QUERY_LEN);

    strncpy(zeroLength1URL, openServerFile_zeroLength1.url, MAX_URL_LEN);
    zeroLength1URL[MAX_URL_LEN] = '\0';
    strncat(zeroLength1URL, buffer, MAX_QUERY_LEN);

    const MockHttpEndpoint typical1EndpointWithParams = {
        .url = typical1URL,
        .responseCode = openServerFile_typical1.responseCode,
        .response = openServerFile_typical1.response,
        .contentLen = openServerFile_typical1.contentLen,
    };

    const MockHttpEndpoint zeroLength1EndpointWithParams = {
        .url = zeroLength1URL,
        .responseCode = openServerFile_zeroLength1.responseCode,
        .response = openServerFile_zeroLength1.response,
        .contentLen = openServerFile_zeroLength1.contentLen,
    };

    err = mock_esp_http_client_add_endpoint(typical1EndpointWithParams);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(zeroLength1EndpointWithParams);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* run test */
    err = openServerFile(&contentLength, client, openServerFile_zeroLength1.url, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

    err = openServerFile(&contentLength, client, openServerFile_typical1.url, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(55, contentLength); // size of file

    expected = "abcdefghijklmnopqrstuvwxyz\nhe";
    do {
        contentLength = ESP_HTTP_CLIENT_READ(client, buffer, bufLen - 1);
    } while (contentLength == -ESP_ERR_HTTP_EAGAIN);
    TEST_ASSERT_EQUAL(bufLen - 1, contentLength);
    buffer[bufLen - 1] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that a status code other than 200 returns ESP_ERR_NOT_FOUND and closes 
 * client.
 * 
 * Test case dependencies: openServerFile_typical.
 */
TEST_CASE("openServerFile_nonExistent", "[api_connect]")
{
    DEFINE_OPEN_SERVER_FILE_TYPICAL_1_ENDPOINT(openServerFile_typical1);

    const char *URLNonexistent = CONFIG_API_CONN_TEST_DATA_SERVER CONFIG_API_CONN_TEST_DATA_BASE_URL "/DOES_NOT_EXIST";
    esp_err_t err = ESP_FAIL;
    char endpointURL[MAX_URL_LEN + MAX_QUERY_LEN];
    uint8_t mac[6];
    const int32_t bufLen = 30;
    char buffer[bufLen];
    char *expected;
    int64_t contentLength;

    /* define endpoints with expected query parameters */
    err = esp_base_mac_addr_get(mac);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    strncpy(endpointURL, openServerFile_typical1.url, MAX_URL_LEN);
    endpointURL[MAX_URL_LEN] = '\0';
    snprintf(buffer, MAX_QUERY_LEN, "?id=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    strncat(endpointURL, buffer, MAX_QUERY_LEN);

    const MockHttpEndpoint typical1EndpointWithParams = {
        .url = endpointURL,
        .responseCode = openServerFile_typical1.responseCode,
        .response = openServerFile_typical1.response,
        .contentLen = openServerFile_typical1.contentLen,
    };

    err = mock_esp_http_client_add_endpoint(typical1EndpointWithParams);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(&contentLength, client, URLNonexistent, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

    err = openServerFile(&contentLength, client, openServerFile_typical1.url, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(55, contentLength); // size of file

    expected = "abcdefghijklmnopqrstuvwxyz\nhe";
    do {
        contentLength = ESP_HTTP_CLIENT_READ(client, buffer, bufLen - 1);
    } while (contentLength == -ESP_ERR_HTTP_EAGAIN);
    TEST_ASSERT_EQUAL(bufLen - 1, contentLength);
    buffer[bufLen - 1] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}