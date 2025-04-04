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

#include "api_connect.h"

extern esp_http_client_handle_t client;

#define TAG "test"

/**
 * Tests input guards.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("processOTAAvailableFile_inputGuards", "[ota]")
{
    char *URL;
    esp_err_t err;
    bool available;
    int64_t contentLen;
    int bytes;
    char buf[10];

    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(0);

    available = true;
    err = processOTAAvailableFile(&available, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(true, available);

    available = false;
    err = processOTAAvailableFile(&available, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(false, available);
    
    contentLen = -1;
    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_typical1.json";
    /**
     * processOTAAvailableFile_typical1.json should contain
     * 
     * {
     *     "hardware_version": 2,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_GREATER_THAN(0, contentLen);

    /* test that client is not read */
    err = processOTAAvailableFile(NULL, client);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    do {
        bytes = esp_http_client_read(client, buf, 9);
    } while (bytes == -ESP_ERR_HTTP_EAGAIN);
    buf[9] = '\0';
    TEST_ASSERT_EQUAL(bytes, 9);
    TEST_ASSERT_EQUAL_STRING("{\n    \"ha", buf);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

/**
 * Tests a typical scenario.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("processOTAAvailableFile_typical", "[ota]")
{
    char *URL;
    esp_err_t err;
    bool available;
    int64_t contentLen;

    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(0);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_typical1.json";
    /**
     * processOTAAvailableFile_typical1.json should contain
     * 
     * {
     *     "hardware_version": 2,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(0);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_typical2.json";
    /**
     * processOTAvailableFile_typical2.json should contain
     * 
     * {
     *     "hardware_version": 2,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 0
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(false, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    setHardwareVersion(1);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(0);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_typical3.json";
    /**
     * processOTAvailableFile_typical2.json should contain
     * 
     * {
     *     "hardware_version": 1,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);

    err = esp_http_client_close(client);
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
    char *URL;
    esp_err_t err;
    bool available;
    int64_t contentLen;

    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(0);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_comments0.json";
    /**
     * processOTAAvailableFile_comments0.json should contain
     * 
     * # this is a simple comment
     * {
     *     "hardware_version": 2,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_comments1.json";
    /**
     * processOTAAvailableFile_comments1.json should contain
     * 
     * # this is a simple comment
     * {
     *     "hardware_version": 2,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     * # this is another comment
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_comments2.json";
    /**
     * processOTAAvailableFile_comments2.json should contain
     * 
     * {
     *     "hardware_version": 2, # comment
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0, # comment
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_comments3.json";
    /**
     * processOTAAvailableFile_comments3.json should contain
     * 
     * # cannot contain strings, "string", as values within {}.
     * {
     *     "hardware_version": 2, # ,",{,"}"
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1 # ,
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);

    err = esp_http_client_close(client);
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
    char *URL;
    esp_err_t err;
    bool available;
    int64_t contentLen;

    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(0);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_unordered1.json";
    /**
     * processOTAAvailableFile_unordered1.json should contain
     * 
     * {
     *     "hardware_revision": 0,
     *     "firmware_patch_version": 1,
     *     "hardware_version": 2,
     *     "firmware_minor_version": 6,
     *     "firmware_major_version": 0
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);

    err = esp_http_client_close(client);
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
    char *URL;
    esp_err_t err;
    bool available;
    int64_t contentLen;

    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(0);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_invalid1.json";
    /**
     * processOTAAvailableFile_invalid1.json should contain
     * 
     * {
     *     "hardware_version": 2,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1, # trailing comma
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(false, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_invalid2.json";
    /**
     * processOTAAvailableFile_invalid2.json should contain
     * 
     * {
     *     "hardware_version": 2,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * # missing end bracket
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(false, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_invalid3.json";
    /**
     * processOTAAvailableFile_invalid3.json should contain
     * 
     * {
     *     "hardware_version": "str2", # string in value
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(false, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_invalid4.json";
    /**
     * processOTAAvailableFile_invalid4.json should contain
     * 
     * {
     *     hardware_version: 2, # missing quotes in key
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(false, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_http_client_cancel_request(client);
    TEST_ASSERT_NOT_EQUAL(ESP_FAIL, err);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_invalid5.json";
    /**
     * processOTAAvailableFile_invalid5.json should contain
     * 
     *     "hardware_version": 2,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(false, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_invalid6.json";
    /**
     * processOTAAvailableFile_invalid6.json should contain
     * # missing single quotation mark
     * {
     *     "hardware_version: 2,
     *     "hardware_revision": 0,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(false, available);

    err = esp_http_client_close(client);
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
    char *URL;
    esp_err_t err;
    bool available;
    int64_t contentLen;

    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(0);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_ignore1.json";
    /**
     * processOTAAvailableFile_ignore1.json should contain
     * 
     * {
     *     "ignore_this_key": 37,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1,
     *     "ignore_key": 7,
     *     "hardware_version": 2,
     *     "hardware_revision": 0
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);

    err = esp_http_client_close(client);
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
    char *URL;
    esp_err_t err;
    bool available;
    int64_t contentLen;

    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(0);

    URL = CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL "/processOTAAvailableFile_string1.json";
    /**
     * processOTAAvailableFile_string1.json should contain
     * 
     * # escape characters cannot be parsed, ie. "" cannot be used in strings
     * {
     *     "hardware_version": 2,
     *     "hardware_revision": 0,
     *     ",,{:}:,{ignore}": 234,
     *     "firmware_major_version": 0,
     *     "firmware_minor_version": 6,
     *     "firmware_patch_version": 1
     * }
     */
    err = openServerFile(&contentLen, client, URL, 5);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = processOTAAvailableFile(&available, client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(true, available);

    err = esp_http_client_close(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}