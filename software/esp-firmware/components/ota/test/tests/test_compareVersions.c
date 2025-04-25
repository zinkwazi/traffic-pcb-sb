/**
 * test_compareVersions.c
 * 
 * White box unit testing for ota.c:compareVersions.
 * 
 * Test file dependencies: None.
 */

#include "ota_pi.h"

#include <stdbool.h>

#include "sdkconfig.h"
#include "unity.h"

#include "ota_config.h"
#include "ota_types.h"

extern esp_http_client_handle_t client;

/**
 * Tests edge case values.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("compareVersions_edgeCases", "[ota]")
{
    VersionInfo in;
    UpdateType out;

    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 0;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 0;

    in.hardwareVer = 0;
    in.revisionVer = 0;
    in.majorVer = 0;
    in.minorVer = 0;
    in.patchVer = 0;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 0;
    in.minorVer = 0;
    in.patchVer = 0;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = UINT8_MAX;
    in.revisionVer = UINT8_MAX;
    in.majorVer = UINT8_MAX;
    in.minorVer = UINT8_MAX;
    in.patchVer = UINT8_MAX;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = UINT8_MAX;
    in.minorVer = UINT8_MAX;
    in.patchVer = UINT8_MAX;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_MAJOR, out);
}

/**
 * Tests typical values.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("compareVersions_typical", "[ota]")
{
    VersionInfo in;
    UpdateType out;

    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 0;
    OTA_MAJOR_VERSION = 1;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 3;

    /* no change in version returns false */
    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 1;
    in.minorVer = 6;
    in.patchVer = 3;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* increased patch version returns true */
    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 1;
    in.minorVer = 6;
    in.patchVer = 4;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_PATCH, out);

    /* increased minor version with zeroed patch version */
    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 1;
    in.minorVer = 7;
    in.patchVer = 0;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_MINOR, out);

    /* increased major version with zeroed minor and patch versions */
    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 2;
    in.minorVer = 0;
    in.patchVer = 0;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_MAJOR, out);

    /* mismatching hardware version returns false */
    in.hardwareVer = 3;
    in.revisionVer = 0;
    in.majorVer = 1;
    in.minorVer = 6;
    in.patchVer = 3;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 0;
    in.majorVer = 1;
    in.minorVer = 6;
    in.patchVer = 3;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 1;
    in.minorVer = 6;
    in.patchVer = 3;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);
}

/**
 * More versions that are lower than the current.
 * 
 * Test case dependencies:
 *     - typical
 */
TEST_CASE("compareVersions_lower", "[ota]")
{
    VersionInfo in;
    UpdateType out;

    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 1;
    OTA_MAJOR_VERSION = 3;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 15;

    /* increased minor version, zero patch */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 0;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_MINOR, out);

    /* increased minor version, no change patch */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_MINOR, out);

    /* increased minor version, increased patch */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_MINOR, out);

    /* increased minor version, increased patch */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_MINOR, out);

    /* decreased minor version, increased patch */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* decreased minor version, decreased patch */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* decreased major version, increased patch */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* decreased major version, no change patch */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* decreased major version, decreased patch */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* decreased major version, increased minor */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* decreased major version, decreased minor */
    in.hardwareVer = 2;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 5;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);
}

/**
 * Tests that mismatched hardware versions always returns false.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("compareVersions_mismatch", "[ota]")
{
    VersionInfo in;
    UpdateType out;

    OTA_HARDWARE_VERSION = 2;
    OTA_REVISION_VERSION = 1;
    OTA_MAJOR_VERSION = 3;
    OTA_MINOR_VERSION = 6;
    OTA_PATCH_VERSION = 15;

    /* test lower revision */
    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 0;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 2;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 0;
    in.majorVer = 2;
    in.minorVer = 5;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* test higher revision  */

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 0;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 2;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 2;
    in.revisionVer = 2;
    in.majorVer = 2;
    in.minorVer = 5;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* test lower hardware version  */

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 0;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 1;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 5;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    /* test higher hardware version  */

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 0;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 7;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 3;
    in.minorVer = 5;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 16;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 14;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 7;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 6;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);

    in.hardwareVer = 3;
    in.revisionVer = 1;
    in.majorVer = 2;
    in.minorVer = 5;
    in.patchVer = 15;

    out = compareVersions(in);
    TEST_ASSERT_EQUAL(UPDATE_NONE, out);
}