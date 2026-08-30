/**
 * test_led_matrix.c
 *
 * Created On: 8/29/26
 * Author: Jaden Baptista
 *
 * Unit tests for led_matrix.h. These ensure that fakes match real hardware behavior.
 *
 * @note These tests run against real IS31FL3741A hardware (see the
 * test_hardware executable target), not a mock. Every test that exercises a
 * setter therefore follows a strict save/set/verify/restore pattern so it
 * leaves the matrix ICs in the state it found them in.
 *
 * @warning matGetSWxSetting is intentionally tested read-only, with no call
 * to matSetSWxSetting. The SWx setting determines how many pins are
 * configured as SW (switch/row) outputs versus dedicated CS (current sink)
 * pins. This board's LED matrix is wired for one fixed topology; changing
 * SWx away from that topology can turn a pin the board expects to be
 * high-impedance into an actively driven output (or vice versa), which can
 * drive two outputs against each other through an LED and short the board.
 * Do not add a round-trip test for this setting without first confirming the
 * value is safe for the physical board wiring.
 */

#include "led_matrix.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "unity.h"

#include "esp_err.h"

#include "led_registers.h"

#define TEST_GROUP "[led_matrix]"

#if CONFIG_HARDWARE_VERSION == 1

/* testing of hardware is unsupported */

#elif CONFIG_HARDWARE_VERSION == 2

static const Matrix sAllMatrices[] = {MATRIX1, MATRIX2, MATRIX3, MATRIX4};
#define NUM_MATRICES (sizeof(sAllMatrices) / sizeof(sAllMatrices[0]))

/* An LED number guaranteed to be present on any populated V2.0/V2.1 board. */
#define TEST_LED_NUM 1

/* The ID Register (FCh) reads back as the device's own slave address in
shifted 8-bit form (ie. the 7-bit address used for bus addressing, shifted
left by one) -- confirmed against the datasheet's own worked example ("if
ADDR pin connects to GND, read result is 0x60", where 0x60 == 0x30 << 1).
These must match (MAT1_ADDR..MAT4_ADDR in led_matrix.c) << 1: mat1/mat3
share one address (bus 1 / bus 2), and mat2/mat4 share the other. They let
this test confirm the chip actually responding at each address is a real,
correctly strapped IS31FL3741A. */
#define EXPECTED_MAT1_ID 0x60
#define EXPECTED_MAT2_ID 0x66
#define EXPECTED_MAT3_ID 0x60
#define EXPECTED_MAT4_ID 0x66

/**
 * @brief Ensures the led_matrix component is initialized, independent of
 * test execution order. There is no deinit function, so once initialized the
 * matrix stays initialized for the rest of the test binary.
 */
static void ensureLedMatrixInitialized(void)
{
    if (getLedMatrixStatus() != ESP_OK)
    {
        TEST_ASSERT_EQUAL(ESP_OK, initLedMatrix());
    }
}

TEST_CASE("initLedMatrix_succeeds_thenRejectsReinit", TEST_GROUP)
{
    esp_err_t err = initLedMatrix();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = getLedMatrixStatus();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = initLedMatrix();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, err);
}

/* ------------------------------------------------------------------------
 * Argument validation. These never reach the I2C bus, so they are safe to
 * run in any order and cannot affect the hardware.
 * ---------------------------------------------------------------------- */

TEST_CASE("matGetOperatingMode_rejectsNullAndInvalidMatrix", TEST_GROUP)
{
    ensureLedMatrixInitialized();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetOperatingMode(NULL, MATRIX1));

    enum Operation setting;
    esp_err_t err = matGetOperatingMode(&setting, (Matrix) 99);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(MATRIX_OPERATION_MAX, setting);
}

TEST_CASE("matGetOpenShortDetection_rejectsNullAndInvalidMatrix", TEST_GROUP)
{
    ensureLedMatrixInitialized();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetOpenShortDetection(NULL, MATRIX1));

    enum ShortDetectionEnable setting;
    esp_err_t err = matGetOpenShortDetection(&setting, (Matrix) 99);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(MATRIX_SHORT_DETECTION_EN_MAX, setting);
}

TEST_CASE("matGetLogicLevel_rejectsNullAndInvalidMatrix", TEST_GROUP)
{
    ensureLedMatrixInitialized();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetLogicLevel(NULL, MATRIX1));

    enum LogicLevel setting;
    esp_err_t err = matGetLogicLevel(&setting, (Matrix) 99);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(MATRIX_LOGIC_LEVEL_MAX, setting);
}

TEST_CASE("matGetSWxSetting_rejectsNullAndInvalidMatrix", TEST_GROUP)
{
    ensureLedMatrixInitialized();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetSWxSetting(NULL, MATRIX1));

    enum SWXSetting setting;
    esp_err_t err = matGetSWxSetting(&setting, (Matrix) 99);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(MATRIX_SWXSETTING_MAX, setting);
}

TEST_CASE("matGetGlobalCurrentControl_rejectsNullAndInvalidMatrix", TEST_GROUP)
{
    ensureLedMatrixInitialized();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetGlobalCurrentControl(NULL, MATRIX1));

    uint8_t value;
    esp_err_t err = matGetGlobalCurrentControl(&value, (Matrix) 99);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("matGetResistorPullupSetting_rejectsNullAndInvalidMatrix", TEST_GROUP)
{
    ensureLedMatrixInitialized();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetResistorPullupSetting(NULL, MATRIX1));

    enum ResistorSetting setting;
    esp_err_t err = matGetResistorPullupSetting(&setting, (Matrix) 99);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(MATRIX_RESISTORSETTING_MAX, setting);
}

TEST_CASE("matGetResistorPulldownSetting_rejectsNullAndInvalidMatrix", TEST_GROUP)
{
    ensureLedMatrixInitialized();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetResistorPulldownSetting(NULL, MATRIX1));

    enum ResistorSetting setting;
    esp_err_t err = matGetResistorPulldownSetting(&setting, (Matrix) 99);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_EQUAL(MATRIX_RESISTORSETTING_MAX, setting);
}

TEST_CASE("matGetColor_rejectsNullAndInvalidLedNum", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    uint8_t red, green, blue;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetColor(TEST_LED_NUM, NULL, &green, &blue));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetColor(TEST_LED_NUM, &red, NULL, &blue));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetColor(TEST_LED_NUM, &red, &green, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetColor(0, &red, &green, &blue));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetColor(MAX_NUM_LEDS_REG + 1, &red, &green, &blue));
}

TEST_CASE("matGetScaling_rejectsNullAndInvalidLedNum", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    uint8_t red, green, blue;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetScaling(TEST_LED_NUM, NULL, &green, &blue));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetScaling(TEST_LED_NUM, &red, NULL, &blue));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetScaling(TEST_LED_NUM, &red, &green, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetScaling(0, &red, &green, &blue));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetScaling(MAX_NUM_LEDS_REG + 1, &red, &green, &blue));
}

/* ------------------------------------------------------------------------
 * Read-only hardware check. SWx setting is never written by these tests
 * (see file header); this only confirms the getter decodes the chip's
 * existing configuration into a valid enum value.
 * ---------------------------------------------------------------------- */

TEST_CASE("matGetSWxSetting_readsValidValueFromHardware", TEST_GROUP)
{
    ensureLedMatrixInitialized();

    for (size_t i = 0; i < NUM_MATRICES; i++)
    {
        enum SWXSetting setting;
        esp_err_t err = matGetSWxSetting(&setting, sAllMatrices[i]);
        TEST_ASSERT_EQUAL(ESP_OK, err);
        TEST_ASSERT_LESS_THAN(MATRIX_SWXSETTING_MAX, setting);
    }
}

TEST_CASE("matGetDeviceID_rejectsNullAndInvalidMatrix", TEST_GROUP)
{
    ensureLedMatrixInitialized();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, matGetDeviceID(NULL, MATRIX1));

    uint8_t id;
    esp_err_t err = matGetDeviceID(&id, (Matrix) 99);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/**
 * @brief Shared body for the per-matrix chip identification tests below.
 * The ID Register (FCh) is a global register: the datasheet defines its
 * value as the device's own I2C slave address, fixed by ADDR-pin strapping.
 * Reading it back and comparing it to the address this driver expects for
 * that matrix confirms the physical part responding there is a real,
 * correctly-strapped IS31FL3741A and not, eg. a different/blank part that
 * still ACKs I2C transactions but doesn't implement the registers this
 * driver assumes (which would explain an I2C write reporting ESP_OK while
 * silently doing nothing on-chip). The value is printed regardless of
 * pass/fail so a mismatch is visible without needing to add ad hoc logging.
 */
static void checkDeviceIDOnMatrix(Matrix matrix, uint8_t expectedID)
{
    uint8_t id;
    esp_err_t err = matGetDeviceID(&id, matrix);
    printf("matGetDeviceID: matrix=%d err=0x%x id=0x%02X (expected 0x%02X)\n",
           (int) matrix, err, id, expectedID);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(expectedID, id);
}

TEST_CASE("matGetDeviceID_identifiesMatrix1", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    checkDeviceIDOnMatrix(MATRIX1, EXPECTED_MAT1_ID);
}

TEST_CASE("matGetDeviceID_identifiesMatrix2", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    checkDeviceIDOnMatrix(MATRIX2, EXPECTED_MAT2_ID);
}

TEST_CASE("matGetDeviceID_identifiesMatrix3", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    checkDeviceIDOnMatrix(MATRIX3, EXPECTED_MAT3_ID);
}

TEST_CASE("matGetDeviceID_identifiesMatrix4", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    checkDeviceIDOnMatrix(MATRIX4, EXPECTED_MAT4_ID);
}

/* ------------------------------------------------------------------------
 * Round-trip tests. Each of these reads the matrix's current value first,
 * writes a small, valid test value, verifies the getter reflects it, then
 * restores the original value before returning. None of these settings
 * affect which physical pins are driven or how (unlike SWx), so changing
 * them briefly does not risk shorting the board.
 * ---------------------------------------------------------------------- */

TEST_CASE("matGetOperatingMode_returnsValueSetBySetter", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    enum Operation original;
    TEST_ASSERT_EQUAL(ESP_OK, matGetOperatingMode(&original, MATRIX1));

    /* software shutdown disables all output drivers; it cannot short anything */
    TEST_ASSERT_EQUAL(ESP_OK, matSetOperatingMode(SOFTWARE_SHUTDOWN));
    for (size_t i = 0; i < NUM_MATRICES; i++)
    {
        enum Operation setting;
        TEST_ASSERT_EQUAL(ESP_OK, matGetOperatingMode(&setting, sAllMatrices[i]));
        TEST_ASSERT_EQUAL(SOFTWARE_SHUTDOWN, setting);
    }

    TEST_ASSERT_EQUAL(ESP_OK, matSetOperatingMode(original));
    enum Operation restored;
    TEST_ASSERT_EQUAL(ESP_OK, matGetOperatingMode(&restored, MATRIX1));
    TEST_ASSERT_EQUAL(original, restored);
}

TEST_CASE("matGetOpenShortDetection_returnsValueSetBySetter", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    enum ShortDetectionEnable original;
    TEST_ASSERT_EQUAL(ESP_OK, matGetOpenShortDetection(&original, MATRIX1));

    enum ShortDetectionEnable testValue =
        (original == DISABLE_DETECTION) ? OPEN_DETECTION : DISABLE_DETECTION;

    TEST_ASSERT_EQUAL(ESP_OK, matSetOpenShortDetection(testValue));
    for (size_t i = 0; i < NUM_MATRICES; i++)
    {
        enum ShortDetectionEnable setting;
        TEST_ASSERT_EQUAL(ESP_OK, matGetOpenShortDetection(&setting, sAllMatrices[i]));
        TEST_ASSERT_EQUAL(testValue, setting);
    }

    TEST_ASSERT_EQUAL(ESP_OK, matSetOpenShortDetection(original));
    enum ShortDetectionEnable restored;
    TEST_ASSERT_EQUAL(ESP_OK, matGetOpenShortDetection(&restored, MATRIX1));
    TEST_ASSERT_EQUAL(original, restored);
}

TEST_CASE("matGetLogicLevel_returnsValueSetBySetter", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    enum LogicLevel original;
    TEST_ASSERT_EQUAL(ESP_OK, matGetLogicLevel(&original, MATRIX1));

    enum LogicLevel testValue = (original == STANDARD) ? ALTERNATE : STANDARD;

    TEST_ASSERT_EQUAL(ESP_OK, matSetLogicLevel(testValue));
    for (size_t i = 0; i < NUM_MATRICES; i++)
    {
        enum LogicLevel setting;
        TEST_ASSERT_EQUAL(ESP_OK, matGetLogicLevel(&setting, sAllMatrices[i]));
        TEST_ASSERT_EQUAL(testValue, setting);
    }

    TEST_ASSERT_EQUAL(ESP_OK, matSetLogicLevel(original));
    enum LogicLevel restored;
    TEST_ASSERT_EQUAL(ESP_OK, matGetLogicLevel(&restored, MATRIX1));
    TEST_ASSERT_EQUAL(original, restored);
}

TEST_CASE("matGetResistorPullupSetting_returnsValueSetBySetter", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    enum ResistorSetting original;
    TEST_ASSERT_EQUAL(ESP_OK, matGetResistorPullupSetting(&original, MATRIX1));

    enum ResistorSetting testValue = (original == ONE_K) ? TWO_K : ONE_K;

    TEST_ASSERT_EQUAL(ESP_OK, matSetResistorPullupSetting(testValue));
    for (size_t i = 0; i < NUM_MATRICES; i++)
    {
        enum ResistorSetting setting;
        TEST_ASSERT_EQUAL(ESP_OK, matGetResistorPullupSetting(&setting, sAllMatrices[i]));
        TEST_ASSERT_EQUAL(testValue, setting);
    }

    TEST_ASSERT_EQUAL(ESP_OK, matSetResistorPullupSetting(original));
    enum ResistorSetting restored;
    TEST_ASSERT_EQUAL(ESP_OK, matGetResistorPullupSetting(&restored, MATRIX1));
    TEST_ASSERT_EQUAL(original, restored);
}

TEST_CASE("matGetResistorPulldownSetting_returnsValueSetBySetter", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    enum ResistorSetting original;
    TEST_ASSERT_EQUAL(ESP_OK, matGetResistorPulldownSetting(&original, MATRIX1));

    enum ResistorSetting testValue = (original == ONE_K) ? TWO_K : ONE_K;

    TEST_ASSERT_EQUAL(ESP_OK, matSetResistorPulldownSetting(testValue));
    for (size_t i = 0; i < NUM_MATRICES; i++)
    {
        enum ResistorSetting setting;
        TEST_ASSERT_EQUAL(ESP_OK, matGetResistorPulldownSetting(&setting, sAllMatrices[i]));
        TEST_ASSERT_EQUAL(testValue, setting);
    }

    TEST_ASSERT_EQUAL(ESP_OK, matSetResistorPulldownSetting(original));
    enum ResistorSetting restored;
    TEST_ASSERT_EQUAL(ESP_OK, matGetResistorPulldownSetting(&restored, MATRIX1));
    TEST_ASSERT_EQUAL(original, restored);
}

/* PWM frequency select round-trip tests removed: it never worked on the
LCSC-sourced IS31FL3741A-marked chips this was tested against (two batches). */

TEST_CASE("matGetGlobalCurrentControl_returnsValueSetBySetter", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    uint8_t original;
    TEST_ASSERT_EQUAL(ESP_OK, matGetGlobalCurrentControl(&original, MATRIX1));

    /* No LED PWM duty has been set to a nonzero value by any test at this
       point, so no current actually flows regardless of this setting;
       still keep the test value low to be conservative. */
    uint8_t testValue = (original == 0x10) ? 0x11 : 0x10;

    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(testValue));
    for (size_t i = 0; i < NUM_MATRICES; i++)
    {
        uint8_t value;
        TEST_ASSERT_EQUAL(ESP_OK, matGetGlobalCurrentControl(&value, sAllMatrices[i]));
        TEST_ASSERT_EQUAL(testValue, value);
    }

    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(original));
    uint8_t restored;
    TEST_ASSERT_EQUAL(ESP_OK, matGetGlobalCurrentControl(&restored, MATRIX1));
    TEST_ASSERT_EQUAL(original, restored);
}

TEST_CASE("matGetColor_returnsValueSetBySetter", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    uint8_t origRed, origGreen, origBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetColor(TEST_LED_NUM, &origRed, &origGreen, &origBlue));

    /* minimal nonzero brightness: enough to exercise real register writes
       without meaningfully lighting the LED or drawing current */
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));
    uint8_t red, green, blue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetColor(TEST_LED_NUM, &red, &green, &blue));
    TEST_ASSERT_EQUAL(0x01, red);
    TEST_ASSERT_EQUAL(0x00, green);
    TEST_ASSERT_EQUAL(0x00, blue);

    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, origRed, origGreen, origBlue));
    uint8_t restoredRed, restoredGreen, restoredBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetColor(TEST_LED_NUM, &restoredRed, &restoredGreen, &restoredBlue));
    TEST_ASSERT_EQUAL(origRed, restoredRed);
    TEST_ASSERT_EQUAL(origGreen, restoredGreen);
    TEST_ASSERT_EQUAL(origBlue, restoredBlue);
}

TEST_CASE("matGetScaling_returnsValueSetBySetter", TEST_GROUP)
{
    ensureLedMatrixInitialized();
    uint8_t origRed, origGreen, origBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetScaling(TEST_LED_NUM, &origRed, &origGreen, &origBlue));

    /* the LED is not being driven to a visible color by this test, so a low
       scaling test value carries no risk of overcurrent */
    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(TEST_LED_NUM, 0x01, 0x01, 0x01));
    uint8_t red, green, blue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetScaling(TEST_LED_NUM, &red, &green, &blue));
    TEST_ASSERT_EQUAL(0x01, red);
    TEST_ASSERT_EQUAL(0x01, green);
    TEST_ASSERT_EQUAL(0x01, blue);

    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(TEST_LED_NUM, origRed, origGreen, origBlue));
    uint8_t restoredRed, restoredGreen, restoredBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetScaling(TEST_LED_NUM, &restoredRed, &restoredGreen, &restoredBlue));
    TEST_ASSERT_EQUAL(origRed, restoredRed);
    TEST_ASSERT_EQUAL(origGreen, restoredGreen);
    TEST_ASSERT_EQUAL(origBlue, restoredBlue);
}

#else
#error "Unsupported hardware version!"
#endif
