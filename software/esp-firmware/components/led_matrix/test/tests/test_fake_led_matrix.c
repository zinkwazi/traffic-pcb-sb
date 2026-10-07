/**
 * test_fake_led_matrix.c
 *
 * Unit tests for the fault injection functions of the led_matrix fake.
 *
 * @note These only run when the led_matrix fake is enabled
 * (CONFIG_FAKE_LED_MATRIX), such as in the test_refresh configuration.
 * Each test resets faults before and after itself so a failing test
 * does not leave faults pending for other tests. None of these tests
 * initialize the led_matrix component (the fake does not require it),
 * so they do not affect tests of initLedMatrix.
 */

#include "sdkconfig.h"
#if defined(CONFIG_FAKE_LED_MATRIX)

#include "led_matrix.h"

#include <stdint.h>

#include "unity.h"

#include "esp_err.h"

#define TEST_GROUP "[fake_led_matrix]"

#if CONFIG_HARDWARE_VERSION == 1

/* testing of hardware is unsupported */

#elif CONFIG_HARDWARE_VERSION == 2

/* An LED number guaranteed to be present on any populated V2.0/V2.1 board. */
#define TEST_LED_NUM 1

/* An error a real I2C transaction could return, distinct from argument errors. */
#define INJECTED_ERR ESP_ERR_TIMEOUT

/**
 * @brief Calls the given function of led_matrix.h with valid arguments.
 *
 * @returns The value returned by the function.
 */
static esp_err_t callWithValidArgs(FakeMatFunction func)
{
    enum Operation operation;
    enum ShortDetectionEnable shortDetection;
    enum LogicLevel logicLevel;
    enum SWXSetting swxSetting;
    enum ResistorSetting resistorSetting;
    uint8_t value, red, green, blue;

    switch (func)
    {
        case FAKE_MAT_INIT_LED_MATRIX:
            return initLedMatrix();
        case FAKE_MAT_GET_LED_MATRIX_STATUS:
            return getLedMatrixStatus();
        case FAKE_MAT_SET_OPERATING_MODE:
            return matSetOperatingMode(NORMAL_OPERATION);
        case FAKE_MAT_GET_OPERATING_MODE:
            return matGetOperatingMode(&operation, MATRIX1);
        case FAKE_MAT_SET_OPEN_SHORT_DETECTION:
            return matSetOpenShortDetection(DISABLE_DETECTION);
        case FAKE_MAT_GET_OPEN_SHORT_DETECTION:
            return matGetOpenShortDetection(&shortDetection, MATRIX1);
        case FAKE_MAT_SET_LOGIC_LEVEL:
            return matSetLogicLevel(STANDARD);
        case FAKE_MAT_GET_LOGIC_LEVEL:
            return matGetLogicLevel(&logicLevel, MATRIX1);
        case FAKE_MAT_SET_SWX_SETTING:
            return matSetSWxSetting(NINE);
        case FAKE_MAT_GET_SWX_SETTING:
            return matGetSWxSetting(&swxSetting, MATRIX1);
        case FAKE_MAT_SET_GLOBAL_CURRENT_CONTROL:
            return matSetGlobalCurrentControl(0x10);
        case FAKE_MAT_GET_GLOBAL_CURRENT_CONTROL:
            return matGetGlobalCurrentControl(&value, MATRIX1);
        case FAKE_MAT_SET_RESISTOR_PULLUP_SETTING:
            return matSetResistorPullupSetting(ONE_K);
        case FAKE_MAT_GET_RESISTOR_PULLUP_SETTING:
            return matGetResistorPullupSetting(&resistorSetting, MATRIX1);
        case FAKE_MAT_SET_RESISTOR_PULLDOWN_SETTING:
            return matSetResistorPulldownSetting(ONE_K);
        case FAKE_MAT_GET_RESISTOR_PULLDOWN_SETTING:
            return matGetResistorPulldownSetting(&resistorSetting, MATRIX1);
        case FAKE_MAT_RESET:
            return matReset();
        case FAKE_MAT_GET_DEVICE_ID:
            return matGetDeviceID(&value, MATRIX1);
        case FAKE_MAT_SET_COLOR:
            return matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00);
        case FAKE_MAT_GET_COLOR:
            return matGetColor(TEST_LED_NUM, &red, &green, &blue);
        case FAKE_MAT_SET_SCALING:
            return matSetScaling(TEST_LED_NUM, 0x01, 0x01, 0x01);
        case FAKE_MAT_GET_SCALING:
            return matGetScaling(TEST_LED_NUM, &red, &green, &blue);
        case FAKE_MAT_SET_GCC_BY_AMBIENT_LIGHT:
            return matSetGCCByAmbientLight();
        default:
            TEST_FAIL_MESSAGE("callWithValidArgs is missing a FakeMatFunction");
            return ESP_FAIL;
    }
}

TEST_CASE("fakeMatInjectFault_rejectsInvalidArgs", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, fakeMatInjectFault(FAKE_MAT_FUNCTION_MAX, INJECTED_ERR, 0, 1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, fakeMatInjectFault((FakeMatFunction) -1, INJECTED_ERR, 0, 1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, fakeMatInjectFault(FAKE_MAT_SET_COLOR, ESP_OK, 0, 1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, fakeMatInjectFault(FAKE_MAT_SET_COLOR, INJECTED_ERR, 0, 0));

    fakeMatResetFaults();
}

TEST_CASE("fakeMatGetCallCount_returnsZeroForInvalidFunction", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL_UINT32(0, fakeMatGetCallCount(FAKE_MAT_FUNCTION_MAX));
    TEST_ASSERT_EQUAL_UINT32(0, fakeMatGetCallCount((FakeMatFunction) -1));

    fakeMatResetFaults();
}

TEST_CASE("fakeMatInjectFault_everyFunctionReturnsInjectedError", TEST_GROUP)
{

    for (uint32_t i = 0; i < FAKE_MAT_FUNCTION_MAX; i++)
    {
        fakeMatResetFaults();
        TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault((FakeMatFunction) i, INJECTED_ERR, 0, 1));
        TEST_ASSERT_EQUAL_MESSAGE(INJECTED_ERR, callWithValidArgs((FakeMatFunction) i),
                                  "function did not return injected error");
        TEST_ASSERT_EQUAL_UINT32(1, fakeMatGetCallCount((FakeMatFunction) i));
    }

    fakeMatResetFaults();
}

TEST_CASE("fakeMatInjectFault_failsThenRecovers", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_SET_COLOR, INJECTED_ERR, 0, 2));
    TEST_ASSERT_EQUAL(INJECTED_ERR, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));
    TEST_ASSERT_EQUAL(INJECTED_ERR, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));

    fakeMatResetFaults();
}

TEST_CASE("fakeMatInjectFault_succeedsBeforeFailing", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_SET_COLOR, INJECTED_ERR, 2, 1));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));
    TEST_ASSERT_EQUAL(INJECTED_ERR, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));

    fakeMatResetFaults();
}

TEST_CASE("fakeMatInjectFault_returnsGivenError", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_SET_COLOR, ESP_FAIL, 0, 1));
    TEST_ASSERT_EQUAL(ESP_FAIL, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));

    fakeMatResetFaults();
}

TEST_CASE("fakeMatInjectFault_replacesPendingFault", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_SET_COLOR, ESP_FAIL, 0, 5));
    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_SET_COLOR, INJECTED_ERR, 0, 1));
    TEST_ASSERT_EQUAL(INJECTED_ERR, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));

    fakeMatResetFaults();
}

TEST_CASE("fakeMatInjectFault_onlyAffectsTargetFunction", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_SET_SCALING, INJECTED_ERR, 0, 1));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));
    TEST_ASSERT_EQUAL(INJECTED_ERR, matSetScaling(TEST_LED_NUM, 0x01, 0x01, 0x01));

    fakeMatResetFaults();
}

TEST_CASE("fakeMatInjectFault_failingSetterHasNoSideEffects", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x02, 0x03));
    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_SET_COLOR, INJECTED_ERR, 0, 1));
    TEST_ASSERT_EQUAL(INJECTED_ERR, matSetColor(TEST_LED_NUM, 0x04, 0x05, 0x06));

    uint8_t red, green, blue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetColor(TEST_LED_NUM, &red, &green, &blue));
    TEST_ASSERT_EQUAL(0x01, red);
    TEST_ASSERT_EQUAL(0x02, green);
    TEST_ASSERT_EQUAL(0x03, blue);

    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(0x10));
    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_RESET, INJECTED_ERR, 0, 1));
    TEST_ASSERT_EQUAL(INJECTED_ERR, matReset());

    uint8_t value;
    TEST_ASSERT_EQUAL(ESP_OK, matGetGlobalCurrentControl(&value, MATRIX1));
    TEST_ASSERT_EQUAL(0x10, value);

    fakeMatResetFaults();
}

TEST_CASE("fakeMatInjectFault_failingGetterLeavesOutputsUntouched", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_GET_COLOR, INJECTED_ERR, 0, 1));
    uint8_t red = 0xAA, green = 0xBB, blue = 0xCC;
    TEST_ASSERT_EQUAL(INJECTED_ERR, matGetColor(TEST_LED_NUM, &red, &green, &blue));
    TEST_ASSERT_EQUAL(0xAA, red);
    TEST_ASSERT_EQUAL(0xBB, green);
    TEST_ASSERT_EQUAL(0xCC, blue);

    fakeMatResetFaults();
}

TEST_CASE("fakeMatInjectFault_failingInitLeavesStatusUnchanged", TEST_GROUP)
{
    fakeMatResetFaults();

    esp_err_t statusBefore = getLedMatrixStatus();
    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_INIT_LED_MATRIX, INJECTED_ERR, 0, 1));
    TEST_ASSERT_EQUAL(INJECTED_ERR, initLedMatrix());
    TEST_ASSERT_EQUAL(statusBefore, getLedMatrixStatus());

    fakeMatResetFaults();
}

TEST_CASE("fakeMatGetCallCount_countsSucceedingAndFailingCalls", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL_UINT32(0, fakeMatGetCallCount(FAKE_MAT_SET_COLOR));
    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_SET_COLOR, INJECTED_ERR, 1, 1));
    (void) matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00); /* succeeds */
    (void) matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00); /* fails */
    (void) matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00); /* succeeds */
    TEST_ASSERT_EQUAL_UINT32(3, fakeMatGetCallCount(FAKE_MAT_SET_COLOR));

    /* invalid arguments still count as a call */
    (void) matSetColor(0, 0x01, 0x00, 0x00);
    TEST_ASSERT_EQUAL_UINT32(4, fakeMatGetCallCount(FAKE_MAT_SET_COLOR));
    TEST_ASSERT_EQUAL_UINT32(0, fakeMatGetCallCount(FAKE_MAT_SET_SCALING));

    fakeMatResetFaults();
}

TEST_CASE("fakeMatResetFaults_clearsPendingFaultsAndCallCounts", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_OK, fakeMatInjectFault(FAKE_MAT_SET_COLOR, INJECTED_ERR, 1, 1));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));

    fakeMatResetFaults();
    TEST_ASSERT_EQUAL_UINT32(0, fakeMatGetCallCount(FAKE_MAT_SET_COLOR));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x00, 0x00));

    fakeMatResetFaults();
}

TEST_CASE("fakeMatResetFaults_doesNotChangeMatrixState", TEST_GROUP)
{
    fakeMatResetFaults();

    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, 0x01, 0x02, 0x03));
    fakeMatResetFaults();

    uint8_t red, green, blue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetColor(TEST_LED_NUM, &red, &green, &blue));
    TEST_ASSERT_EQUAL(0x01, red);
    TEST_ASSERT_EQUAL(0x02, green);
    TEST_ASSERT_EQUAL(0x03, blue);

    fakeMatResetFaults();
}

#else
#error "Unsupported hardware version!"
#endif

#endif /* defined(CONFIG_FAKE_LED_MATRIX) */
