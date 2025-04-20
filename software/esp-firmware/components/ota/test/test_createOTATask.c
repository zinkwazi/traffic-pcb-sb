/**
 * test_createOTATask.c
 * 
 * White-box unit tests for ota.c:createOTATask. This also serves to test
 * the OTA task itself a bit.
 */

#include "unity.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "ota.h"
#include "ota_pi.h"

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