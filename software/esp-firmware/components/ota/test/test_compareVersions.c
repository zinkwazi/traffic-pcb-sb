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

/**
 * More versions that are lower than the current.
 * 
 * Test case dependencies:
 *     - typical
 */
TEST_CASE("compareVersions_lower", "[ota]")
{
    bool output;

    setHardwareVersion(2);
    setHardwareRevision(1);
    setFirmwareMajorVersion(3);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(15);

    output = compareVersions(2, 1, 3, 7, 0);
    TEST_ASSERT_EQUAL(true, output);

    output = compareVersions(2, 1, 3, 7, 15);
    TEST_ASSERT_EQUAL(true, output);

    output = compareVersions(2, 1, 3, 7, 16);
    TEST_ASSERT_EQUAL(true, output);

    output = compareVersions(2, 1, 3, 5, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 1, 3, 5, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 1, 3, 5, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 1, 2, 6, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 1, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 1, 2, 6, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 1, 2, 7, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 1, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 1, 2, 5, 15);
    TEST_ASSERT_EQUAL(false, output);
}

/**
 * Tests that mismatched hardware versions always returns false.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("compareVersions_mismatch", "[ota]")
{
    bool output;

    setHardwareVersion(2);
    setHardwareRevision(1);
    setFirmwareMajorVersion(3);
    setFirmwareMinorVersion(6);
    setFirmwarePatchVersion(15);

    /* test lower revision2*/

    output = compareVersions(2, 0, 3, 7, 0);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 3, 7, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 3, 7, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 3, 5, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 3, 5, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 3, 5, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 2, 6, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 2, 6, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 2, 7, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 0, 2, 5, 15);
    TEST_ASSERT_EQUAL(false, output);

    

    /* test higher revision  */

    output = compareVersions(2, 2, 3, 7, 0);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 3, 7, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 3, 7, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 3, 5, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 3, 5, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 3, 5, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 2, 6, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 2, 6, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 2, 7, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(2, 2, 2, 5, 15);
    TEST_ASSERT_EQUAL(false, output);

    /* test lower hardware version  */

    output = compareVersions(1, 1, 3, 7, 0);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 3, 7, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 3, 7, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 3, 5, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 3, 5, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 3, 5, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 2, 6, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 2, 6, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 2, 7, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(1, 1, 2, 5, 15);
    TEST_ASSERT_EQUAL(false, output);

    /* test higher hardware version  */

    output = compareVersions(3, 1, 3, 7, 0);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 3, 7, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 3, 7, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 3, 5, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 3, 5, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 3, 5, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 2, 6, 16);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 2, 6, 14);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 2, 7, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 2, 6, 15);
    TEST_ASSERT_EQUAL(false, output);

    output = compareVersions(3, 1, 2, 5, 15);
    TEST_ASSERT_EQUAL(false, output);
}