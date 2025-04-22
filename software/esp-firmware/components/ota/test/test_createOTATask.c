/**
 * test_createOTATask.c
 * 
 * White-box unit tests for ota.c:createOTATask. This also serves to test
 * the OTA task itself a bit.
 */

#include "unity.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "ota.h"
#include "ota_pi.h"
#include "mock_indicators.h"

#define TAG "test"

#define URL_BASE CONFIG_OTA_TEST_DATA_SERVER CONFIG_OTA_TEST_DATA_BASE_URL

TEST_CASE("createOTATask_createsTask", "[ota]")
{
    esp_err_t err;
    BaseType_t success;
    SemaphoreHandle_t performedUpdateSema;
    TaskHandle_t otaTask;

    setUpgradeVersionURL(URL_BASE "/createOTATask_version.json");
    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(2);
    setFirmwarePatchVersion(0);

    err = initPerformedUpdateSema();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    setUpdateFails(false);

    err = createOTATask(&otaTask);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    success = xTaskNotify(otaTask, 0xFF, eSetBits); // during testing, ota task
                                                    // will delete itself after
                                                    // handling
    TEST_ASSERT_EQUAL(pdPASS, success);

    performedUpdateSema = getPerformedUpdateSema();
    success = xSemaphoreTake(performedUpdateSema, pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(pdTRUE, success);

    vSemaphoreDelete(performedUpdateSema);
}

/**
 * @brief Tests that vOTATask calls indicator functions as expected.
 * 
 * @requires:
 * - CONFIG_MOCK_INDICATORS is set.
 */
TEST_CASE("vOTATask_indicatesCorrectly", "[ota]")
{
#ifndef CONFIG_MOCK_INDICATORS
    TEST_FAIL_MESSAGE("Indicators component is not mocked!");
#else
#if CONFIG_HARDWARE_VERSION == 1
    /* indication unsupported */
#elif CONFIG_HARDWARE_VERSION == 2
    esp_err_t err;
    BaseType_t success;
    TaskHandle_t otaTask;

    /* indicateOTAUpdateAvailable is called when an update is available */
    setUpgradeVersionURL(URL_BASE "/vOTATask_indicatesCorrectly1.json"); // V2_0 v0.7.5
    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(6); // indicates update available, doesn't auto update
    setFirmwarePatchVersion(0);
    TEST_ASSERT_EQUAL(ESP_OK, initPerformedUpdateSema()); // unused
    setUpdateFails(true); // don't expect to update anything, set for reproducibility
    TEST_ASSERT_EQUAL(ESP_OK, mockIndicatorsStartRecording(2)); // expect 1 calls
    TEST_ASSERT_GREATER_THAN(1, CONFIG_OTA_PRIO); // need to let this task's prio be less than ota task
    vTaskPrioritySet(NULL, CONFIG_OTA_PRIO - 1);
    TEST_ASSERT_EQUAL(ESP_OK, createOTATask(&otaTask));
    /* this task is a lower priority than the OTA task, so it will wake up
    after the OTA task is done initializing and is waiting for a task notification.
    If not, then something has gone wrong in the OTA task and this is blocked forever. */
    vTaskDelay(pdMS_TO_TICKS(1000)); // TODO: don't wait, its bad design for a test
    TEST_ASSERT_EQUAL(false, mockIndicatorsRecordingOverflowed());
    TEST_ASSERT_EQUAL(INDICATE_OTA_AVAILABLE, mockIndicatorsGetRecording(0));
    TEST_ASSERT_EQUAL(INDICATE_RECORDING_END, mockIndicatorsGetRecording(1));
    vTaskDelete(otaTask);
#else
#error "Unsupported hardware version!"
#endif /* CONFIG_HARDWARE_VERSION */
#endif /* CONFIG_MOCK_INDICATORS */
}