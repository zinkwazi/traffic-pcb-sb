/**
 * test_openServerFile.c
 * 
 * Black box unit tests for api_connect.c:openServerFile.
 * 
 * Test file dependencies: None.
 * 
 * Server file dependencies: 
 *     CONFIG_TEST_DATA_BASE_URL/data_north_V1_0_5.csv.
 *     CONFIG_TEST_DATA_BASE_URL/openServerFile_typical.1.
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
    const char *URL = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/data_north_V1_0_5.csv";
    esp_err_t err;
    int64_t contentLength;

    err = openServerFile(NULL, client, URL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = openServerFile(&contentLength, NULL, URL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = openServerFile(&contentLength, client, NULL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = openServerFile(&contentLength, client, URL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = openServerFile(&contentLength, client, URL, -1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/**
 * Tests that a typical use case allows reading via esp_http_client_read.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("openServerFile_typical", "[api_connect]")
{
    const char *URL = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/openServerFile_typical.1";
    esp_err_t err;
    const int32_t bufLen = 30;
    char buffer[bufLen];
    char *expected;
    int64_t contentLength = 0;

    err = openServerFile(&contentLength, client, URL, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(55, contentLength); // size of file

    expected = "abcdefghijklmnopqrstuvwxyz\nhe";
    do {
        contentLength = esp_http_client_read(client, buffer, bufLen - 1);
    } while (contentLength == -ESP_ERR_HTTP_EAGAIN);
    TEST_ASSERT_EQUAL(bufLen - 1, contentLength);
    buffer[bufLen - 1] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that zero content length returns ESP_ERR_NOT_FOUND and closes client.
 *  
 * Test case dependencies: openServerFile_typical.
 */
TEST_CASE("openServerFile_zeroContentLength", "[api_connect]")
{
    const char *URLZeroContent = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/openServerFile_zeroContentLength.1";
    const char *URLTypical = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/openServerFile_typical.1";
    esp_err_t err;
    const int32_t bufLen = 30;
    char buffer[bufLen];
    char *expected;
    int64_t contentLength;

    err = openServerFile(&contentLength, client, URLZeroContent, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

    err = openServerFile(&contentLength, client, URLTypical, RETRY_NUM);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(55, contentLength); // size of file

    expected = "abcdefghijklmnopqrstuvwxyz\nhe";
    do {
        contentLength = esp_http_client_read(client, buffer, bufLen - 1);
    } while (contentLength == -ESP_ERR_HTTP_EAGAIN);
    TEST_ASSERT_EQUAL(bufLen - 1, contentLength);
    buffer[bufLen - 1] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, buffer);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that a status code other than 200 returns ESP_ERR_NOT_FOUND and closes 
 * client.
 * 
 * @bug #1: The function should return ESP_ERR_NOT_SUPPORTED instead of
 *      ESP_ERR_NOT_FOUND when the status code is not 200 due to
 *      esp_http_client_flush_response not behaving as expected. This test case
 *      will fail until the bug is fixed.
 * 
 * Test case dependencies: openServerFile_typical.
 */
// TEST_CASE("openServerFile_nonExistent", "[api_connect]")
// {
//     const char *URLNonexistent = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/DOES_NOT_EXIST";
//     const char *URLTypical = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/openServerFile_typical.1";
//     esp_err_t err = ESP_FAIL;
//     const int32_t bufLen = 30;
//     char buffer[bufLen];
//     char *expected;
//     int64_t contentLength;

//     // don't run this test case until bug #1 is fixed!
//     // err = openServerFile(&contentLength, client, URLNonexistent, RETRY_NUM);
//     TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

//     err = openServerFile(&contentLength, client, URLTypical, RETRY_NUM);
//     TEST_ASSERT_EQUAL(ESP_OK, err);
//     TEST_ASSERT_EQUAL(55, contentLength); // size of file

//     expected = "abcdefghijklmnopqrstuvwxyz\nhe";
//     do {
//         contentLength = esp_http_client_read(client, buffer, bufLen - 1);
//     } while (contentLength == -ESP_ERR_HTTP_EAGAIN);
//     TEST_ASSERT_EQUAL(bufLen - 1, contentLength);
//     buffer[bufLen - 1] = '\0';
//     TEST_ASSERT_EQUAL_STRING(expected, buffer);

//     err = esp_http_client_close(client);
//     TEST_ASSERT_EQUAL(ESP_OK, err);
// }