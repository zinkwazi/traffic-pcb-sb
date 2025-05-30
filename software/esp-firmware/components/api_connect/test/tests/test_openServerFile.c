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
#include "esp_log.h"
#include "esp_http_client.h"
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
    DEFINE_DATA_NORTH_V1_0_5_ENDPOINT;

    esp_err_t err;
    int64_t contentLength;

    err = mock_esp_http_client_add_endpoint(data_north_V1_0_5);
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
    DEFINE_OPEN_SERVER_FILE_TYPICAL_1_ENDPOINT;

    esp_err_t err;
    const int32_t bufLen = 30;
    char buffer[bufLen];
    char *expected;
    int64_t contentLength = 0;

    err = mock_esp_http_client_add_endpoint(openServerFile_typical1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

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
    DEFINE_OPEN_SERVER_FILE_TYPICAL_1_ENDPOINT;
    DEFINE_OPEN_SERVER_FILE_ZERO_LENGTH_1_ENDPOINT;

    esp_err_t err;
    const int32_t bufLen = 30;
    char buffer[bufLen];
    char *expected;
    int64_t contentLength;

    err = mock_esp_http_client_add_endpoint(openServerFile_typical1);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(openServerFile_zeroLength1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

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
    DEFINE_OPEN_SERVER_FILE_TYPICAL_1_ENDPOINT;


    const char *URLNonexistent = CONFIG_API_CONN_TEST_DATA_SERVER CONFIG_API_CONN_TEST_DATA_BASE_URL "/DOES_NOT_EXIST";
    esp_err_t err = ESP_FAIL;
    const int32_t bufLen = 30;
    char buffer[bufLen];
    char *expected;
    int64_t contentLength;

    err = mock_esp_http_client_add_endpoint(openServerFile_typical1);
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