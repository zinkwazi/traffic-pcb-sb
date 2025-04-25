/**
 * test_versionFromKey.c
 * 
 * White box testing for ota.c:versionFromKey.
 * 
 * Test file dependencies: None.
 */

#include "ota_pi.h"

#include <string.h>

#include "esp_err.h"
#include "unity.h"

#include "ota_config.h"

extern esp_http_client_handle_t client;

/**
 * Tests input guards.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("versionFromKey_inputGuards", "[ota]")
{
    VersionType type;
    esp_err_t err;
    const int buflen = 128;
    char buffer[buflen];

    memset(buffer, 0, 128);
    strcpy(buffer, "\"hardware_version\"");
    type = MAJOR;
    err = versionFromKey(NULL, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(MAJOR, type);

    err = versionFromKey(&type, NULL, buflen);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(MAJOR, type);

    err = versionFromKey(&type, buffer, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(MAJOR, type);
}

/**
 * Tests typical successful cases.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("versionFromKey_typical", "[ota]")
{
    VersionType type;
    esp_err_t err;
    const int buflen = 128;
    char buffer[buflen];

    HARDWARE_VERSION_KEY = "hardware_version";
    HARDWARE_REVISION_KEY = "hardware_revision";
    FIRMWARE_MAJOR_KEY = "firmware_major_version";
    FIRMWARE_MINOR_KEY = "firmware_minor_version";
    FIRMWARE_PATCH_KEY = "firmware_patch_version";

    memset(buffer, 0, 128);
    strcpy(buffer, "\"hardware_version\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(HARDWARE, type);

    strcpy(buffer, "\"hardware_revision\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(REVISION, type);

    strcpy(buffer, "\"firmware_major_version\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(MAJOR, type);

    strcpy(buffer, "\"firmware_minor_version\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(MINOR, type);

    strcpy(buffer, "\"firmware_patch_version\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(PATCH, type);
}

/**
 * Tests that keys are found in buffer.
 * 
 * Test case dependencies: versionFromKey_typical.
 */
TEST_CASE("versionFromKey_findsKey", "[ota]")
{
    VersionType type;
    esp_err_t err;
    const int buflen = 128;
    char buffer[buflen];

    HARDWARE_VERSION_KEY = "hardware_version";
    HARDWARE_REVISION_KEY = "hardware_revision";
    FIRMWARE_MAJOR_KEY = "firmware_major_version";
    FIRMWARE_MINOR_KEY = "firmware_minor_version";
    FIRMWARE_PATCH_KEY = "firmware_patch_version";

    memset(buffer, 0, 128);
    strcpy(buffer, "garbage" "\"hardware_version\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(HARDWARE, type);

    strcpy(buffer, "garbage" "\"hardware_revision\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(REVISION, type);

    strcpy(buffer, "garbage" "\"firmware_major_version\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(MAJOR, type);

    strcpy(buffer, "garbage" "\"firmware_minor_version\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(MINOR, type);

    strcpy(buffer, "garbage" "\"firmware_patch_version\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(PATCH, type);
}

/**
 * Tests that keys are found in buffer.
 * 
 * Test case dependencies: versionFromKey_typical.
 */
TEST_CASE("versionFromKey_unknownKey", "[ota]")
{
    VersionType type;
    esp_err_t err;
    const int buflen = 128;
    char buffer[buflen];

    HARDWARE_VERSION_KEY = "hardware_version";
    HARDWARE_REVISION_KEY = "hardware_revision";
    FIRMWARE_MAJOR_KEY = "firmware_major_version";
    FIRMWARE_MINOR_KEY = "firmware_minor_version";
    FIRMWARE_PATCH_KEY = "firmware_patch_version";

    memset(buffer, 0, 128);
    strcpy(buffer, "\"" "this_is_not_a_key" "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(VER_TYPE_UNKNOWN, type);

    strcpy(buffer, "\"" "." "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(VER_TYPE_UNKNOWN, type);

    strcpy(buffer, "\"" "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(VER_TYPE_UNKNOWN, type);

    strcpy(buffer, "\"" " " "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(VER_TYPE_UNKNOWN, type);

    strcpy(buffer, "\"" "\0" "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(VER_TYPE_UNKNOWN, type);
}

/**
 * Tests that keys are found in buffer.
 * 
 * Test case dependencies: versionFromKey_typical.
 */
TEST_CASE("versionFromKey_badFormat", "[ota]")
{
    VersionType type;
    esp_err_t err;
    const int buflen = 128;
    char buffer[buflen];

    HARDWARE_VERSION_KEY = "hardware_version";
    HARDWARE_REVISION_KEY = "hardware_revision";
    FIRMWARE_MAJOR_KEY = "firmware_major_version";
    FIRMWARE_MINOR_KEY = "firmware_minor_version";
    FIRMWARE_PATCH_KEY = "firmware_patch_version";

    memset(buffer, 0, 128);
    strcpy(buffer, "garbage" "\"hardware_version");
    type = MAJOR;
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
    TEST_ASSERT_EQUAL(MAJOR, type);

    memset(buffer, 0, 128);
    strcpy(buffer, "garbage");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
    TEST_ASSERT_EQUAL(MAJOR, type);

    memset(buffer, 0, 128);
    strcpy(buffer, "garbage" "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
    TEST_ASSERT_EQUAL(MAJOR, type);
}