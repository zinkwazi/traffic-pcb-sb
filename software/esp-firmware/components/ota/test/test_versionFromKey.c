/**
 * test_versionFromKey.c
 * 
 * White box testing for ota.c:versionFromKey.
 * 
 * Test file dependencies: None.
 */

#include "ota_config.h"
#include "ota_pi.h"

#include <string.h>

#include "esp_err.h"
#include "unity.h"

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
    strcpy(buffer, "\"" HARDWARE_VERSION_KEY "\"");
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

    memset(buffer, 0, 128);
    strcpy(buffer, "\"" HARDWARE_VERSION_KEY "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(HARDWARE, type);

    strcpy(buffer, "\"" HARDWARE_REVISION_KEY "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(REVISION, type);

    strcpy(buffer, "\"" FIRMWARE_MAJOR_KEY "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(MAJOR, type);

    strcpy(buffer, "\"" FIRMWARE_MINOR_KEY "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(MINOR, type);

    strcpy(buffer, "\"" FIRMWARE_PATCH_KEY "\"");
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

    memset(buffer, 0, 128);
    strcpy(buffer, "garbage" "\"" HARDWARE_VERSION_KEY "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(HARDWARE, type);

    strcpy(buffer, "garbage" "\"" HARDWARE_REVISION_KEY "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(REVISION, type);

    strcpy(buffer, "garbage" "\"" FIRMWARE_MAJOR_KEY "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(MAJOR, type);

    strcpy(buffer, "garbage" "\"" FIRMWARE_MINOR_KEY "\"");
    err = versionFromKey(&type, buffer, buflen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(MINOR, type);

    strcpy(buffer, "garbage" "\"" FIRMWARE_PATCH_KEY "\"");
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

    memset(buffer, 0, 128);
    strcpy(buffer, "garbage" "\"" HARDWARE_VERSION_KEY);
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