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

/**
 * Tests edge case values.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("compareVersions_edgeCases", "[ota]")
{
    bool output;

    setHardwareVersion(CONFIG_HARDWARE_VERSION);
    setHardwareRevision(CONFIG_HARDWARE_REVISION);
    setFirmwareMajorVersion(CONFIG_FIRMWARE_MAJOR_VERSION);
    setFirmwareMinorVersion(CONFIG_FIRMWARE_MINOR_VERSION);
    setFirmwarePatchVersion(CONFIG_FIRMWARE_PATCH_VERSION);

    output = compareVersions(0, 0, 0, 0, 0);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(CONFIG_HARDWARE_VERSION, CONFIG_HARDWARE_REVISION, 0, 0, 0);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(CONFIG_HARDWARE_VERSION, CONFIG_HARDWARE_REVISION, UINT_MAX, UINT_MAX, UINT_MAX);
    TEST_ASSERT_EQUAL(true, output);
}

/**
 * Tests typical values.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("compareVersions_typical", "[ota]")
{
    bool output;
    setHardwareVersion(CONFIG_HARDWARE_VERSION);
    setHardwareRevision(CONFIG_HARDWARE_REVISION);
    setFirmwareMajorVersion(CONFIG_FIRMWARE_MAJOR_VERSION);
    setFirmwareMinorVersion(CONFIG_FIRMWARE_MINOR_VERSION);
    setFirmwarePatchVersion(CONFIG_FIRMWARE_PATCH_VERSION);

    /* no change in version returns false */
    output = compareVersions(CONFIG_HARDWARE_VERSION, 
                             CONFIG_HARDWARE_REVISION, 
                             CONFIG_FIRMWARE_MAJOR_VERSION,
                             CONFIG_FIRMWARE_MINOR_VERSION,
                             CONFIG_FIRMWARE_PATCH_VERSION);
    TEST_ASSERT_EQUAL(false, output);

    /* increased patch version returns true */
    output = compareVersions(CONFIG_HARDWARE_VERSION, 
                             CONFIG_HARDWARE_REVISION, 
                             CONFIG_FIRMWARE_MAJOR_VERSION,
                             CONFIG_FIRMWARE_MINOR_VERSION,
                             CONFIG_FIRMWARE_PATCH_VERSION + 1);
    TEST_ASSERT_EQUAL(true, output);

    /* increased minor version with zeroed patch version */
    output = compareVersions(CONFIG_HARDWARE_VERSION, 
                             CONFIG_HARDWARE_REVISION, 
                             CONFIG_FIRMWARE_MAJOR_VERSION,
                             CONFIG_FIRMWARE_MINOR_VERSION + 1,
                             0);
    TEST_ASSERT_EQUAL(true, output);

    /* increased major version with zeroed minor and patch versions */
    output = compareVersions(CONFIG_HARDWARE_VERSION, 
                             CONFIG_HARDWARE_REVISION, 
                             CONFIG_FIRMWARE_MAJOR_VERSION + 1,
                             0,
                             0);
    TEST_ASSERT_EQUAL(true, output);

    /* mismatching hardware version returns false */
    output = compareVersions(CONFIG_HARDWARE_VERSION + 1, 
                             CONFIG_HARDWARE_REVISION, 
                             CONFIG_FIRMWARE_MAJOR_VERSION,
                             CONFIG_FIRMWARE_MINOR_VERSION,
                             CONFIG_FIRMWARE_PATCH_VERSION);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(CONFIG_HARDWARE_VERSION - 1,
                             CONFIG_HARDWARE_REVISION, 
                             CONFIG_FIRMWARE_MAJOR_VERSION,
                             CONFIG_FIRMWARE_MINOR_VERSION,
                             CONFIG_FIRMWARE_PATCH_VERSION);
    TEST_ASSERT_EQUAL(false, output);

    /* mismatching hardware version returns false */
    output = compareVersions(CONFIG_HARDWARE_VERSION, 
                             CONFIG_HARDWARE_REVISION + 1, 
                             CONFIG_FIRMWARE_MAJOR_VERSION,
                             CONFIG_FIRMWARE_MINOR_VERSION,
                             CONFIG_FIRMWARE_PATCH_VERSION);
    TEST_ASSERT_EQUAL(false, output);
}