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
#include "esp_http_client.h"

#include "api_connect.h"

extern esp_http_client_handle_t client;

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