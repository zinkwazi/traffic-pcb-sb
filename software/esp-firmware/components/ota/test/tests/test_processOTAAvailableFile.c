/**
 * test_processOTAAvailableFile.c
 * 
 * White box testing for ota.c:processOTAAvailableFile
 * 
 * Test file dependencies: 
 *     - test_versionFromKey.c
 *     - test_compareVersions.c
 */

#include "ota_pi.h"
#include "ota_config.h"

#include <stdint.h>

#include "unity.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "wrap_esp_http_client.h"
#include "mock_esp_http_client.h"

#include "utilities.h"
#include "api_connect.h"

#include "resources/processOTAAvailableFileResources.h"

extern esp_http_client_handle_t client;

#define TAG "test"

/**
 * Tests input guards.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("processOTAAvailableFile_inputGuards", "[ota]")
{
    MOCK_ENDPOINT(typical1);

    esp_err_t err;
    bool available;
    bool patch;
    int64_t contentLen;
    int bytes;
    char buf[10];

    /* setup mocks */
    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 0;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 0;

    err = mock_esp_http_client_add_endpoint(typical1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test 1: NULL client */
    available = true;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(false, patch);

    available = false;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(false, available);
    TEST_ASSERT_EQUAL(false, patch);

    /* test 2: NULL available & patch */
    contentLen = -1;
    err = openServerFile(&contentLen, client, typical1.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_GREATER_THAN(0, contentLen);

    err = processOTAAvailableFile(NULL, NULL, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    // ensure client is not read
    bytes = ESP_HTTP_CLIENT_READ(client, buf, 9);
    buf[9] = '\0';
    TEST_ASSERT_EQUAL(bytes, 9);
    TEST_ASSERT_EQUAL_STRING("{\n    \"ha", buf);
    
    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests a typical scenario.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("processOTAAvailableFile_typical", "[ota]")
{
    MOCK_ENDPOINT(typical1);
    MOCK_ENDPOINT(typical2);
    MOCK_ENDPOINT(typical3);

    esp_err_t err;
    bool available;
    bool patch;
    int64_t contentLen;

    /* setup mocks */
    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 0;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 0;

    err = mock_esp_http_client_add_endpoint(typical1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = mock_esp_http_client_add_endpoint(typical2);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = mock_esp_http_client_add_endpoint(typical3);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test 1: typical w/ patch update */
    err = openServerFile(&contentLen, client, typical1.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    available = false;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(true, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test 2: typical w/o patch update */
    err = openServerFile(&contentLen, client, typical2.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    available = true;
    patch = true;
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(false, available);
    TEST_ASSERT_EQUAL(false, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test 3: typical w/ V1_0 */
    OTA_HARDWARE_VERSION = 1;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 0;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 0;

    err = openServerFile(&contentLen, client, typical3.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    available = false;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(true, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that comments are ignored.
 * 
 * Test case dependencies:
 *     - typical
 */
TEST_CASE("processOTAAvailableFile_comments", "[ota]")
{
    MOCK_ENDPOINT(comments0);
    MOCK_ENDPOINT(comments1);
    MOCK_ENDPOINT(comments2);
    MOCK_ENDPOINT(comments3);

    esp_err_t err;
    bool available;
    bool patch;
    int64_t contentLen;

    /* setup mocks */
    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 0;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 0;

    err = mock_esp_http_client_add_endpoint(comments0);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(comments1);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(comments2);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(comments3);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test */
    err = openServerFile(&contentLen, client, comments0.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    available = false;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(true, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(&contentLen, client, comments1.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    available = false;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(true, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(&contentLen, client, comments2.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    available = false;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(true, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(&contentLen, client, comments3.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    available = false;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(true, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that randomly ordered keys are parsed correctly.
 * 
 * Test case dependencies:
 *     - typical
 */
TEST_CASE("processOTAAvailableFile_unordered", "[ota]")
{
    MOCK_ENDPOINT(unordered1);

    esp_err_t err;
    bool available;
    bool patch;
    int64_t contentLen;

    /* setup mocks */
    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 0;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 0;

    err = mock_esp_http_client_add_endpoint(unordered1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test */
    err = openServerFile(&contentLen, client, unordered1.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    available = false;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(true, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that invalidly formatted JSON fails gracefully.
 * 
 * Test case dependencies:
 *     - typical
 *     - comments
 */
TEST_CASE("processOTAAvailableFile_invalid", "[ota]")
{
    MOCK_ENDPOINT(invalid1);
    MOCK_ENDPOINT(invalid2);
    MOCK_ENDPOINT(invalid3);
    MOCK_ENDPOINT(invalid4);
    MOCK_ENDPOINT(invalid5);
    MOCK_ENDPOINT(invalid6);

    esp_err_t err;
    bool available;
    bool patch;
    int64_t contentLen;

    /* setup mocks */
    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 0;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 0;

    err = mock_esp_http_client_add_endpoint(invalid1);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(invalid2);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(invalid3);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(invalid4);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(invalid5);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(invalid6);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test */
    err = openServerFile(&contentLen, client, invalid1.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(&contentLen, client, invalid2.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(&contentLen, client, invalid3.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(&contentLen, client, invalid4.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(&contentLen, client, invalid5.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = openServerFile(&contentLen, client, invalid6.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that non-target keys are skipped.
 * 
 * Test case dependencies:
 *     - typical
 *     - unordered
 */
TEST_CASE("processOTAAvailableFile_ignoresKeys", "[ota]")
{
    MOCK_ENDPOINT(ignore1);

    esp_err_t err;
    bool available;
    bool patch;
    int64_t contentLen;

    /* setup mocks */
    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 0;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 0;

    err = mock_esp_http_client_add_endpoint(ignore1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test */
    err = openServerFile(&contentLen, client, ignore1.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    available = false;
    patch = false;
    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(true, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests that formatting characters in strings does not fail.
 * 
 * Test case dependencies:
 *     - typical
 *     - unordered
 *     - comments
 *     - ingoresKeys
 */
TEST_CASE("processOTAAvailableFile_formattingInString", "[ota]")
{
    MOCK_ENDPOINT(string1);

    esp_err_t err;
    bool available;
    bool patch;
    int64_t contentLen;

    /* setup tests */
    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 0;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 0;

    err = mock_esp_http_client_add_endpoint(string1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test */
    err = openServerFile(&contentLen, client, string1.url, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, &patch, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);
    TEST_ASSERT_EQUAL(true, patch);

    err = ESP_HTTP_CLIENT_CLOSE(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}