/**
 * test_action_queryOTA.c
 * 
 * Unit tests for actions.c:handleActionQueryOTA.
 * 
 * Test file dependencies: None.
 * 
 * Server file dependencies:
 *     CONFIG_ACTIONS_TEST_DATA_BASE_URL/queryOTA_
 */

#include "unity.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "actions.h"
#include "actions_pi.h"
#include "ota_pi.h"

#define URL_BASE CONFIG_ACTIONS_TEST_DATA_SERVER CONFIG_ACTIONS_TEST_DATA_BASE_URL

/**
 * @brief A mock OTA task implementation that sets its parameter to true
 * when a task notification is received.
 * 
 * @param[out] pvParams A SemaphoreHandle_t of a binary semaphore, which
 * this task will give once it receives a task notification.
 */
static void vSendsNotifOTAMock(void *pvParams)
{
    SemaphoreHandle_t sema = (SemaphoreHandle_t) pvParams;

    while (true)
    {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 0)
        {
            xSemaphoreGive(sema);
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Tests that queryOTA action sends a task notification to the OTA
 * task only if a patch update is available.
 */
TEST_CASE("queryOTA_sendsNotif", "[actions]")
{
    SemaphoreHandle_t sema;
    TaskHandle_t otaMockTask;
    BaseType_t success;

    /* test that patch update sends task notification */
    setUpgradeVersionURL(URL_BASE "/queryOTA_sendsNotif1.json"); // patch = 1
    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(2);
    setFirmwarePatchVersion(0);

    sema = xSemaphoreCreateBinary();

    success = xTaskCreate(vSendsNotifOTAMock, "otaMock", 2000, sema, 10, &otaMockTask);
    TEST_ASSERT_EQUAL(pdPASS, success);

    setOTATask(otaMockTask);
    
    handleActionQueryOTA();

    success = xSemaphoreTake(sema, pdMS_TO_TICKS(1000)); // wait 1 second
    TEST_ASSERT_EQUAL(pdTRUE, success);

    vTaskDelete(otaMockTask);
    vSemaphoreDelete(sema);

    /* test that minor updates does not send task notification */
    setUpgradeVersionURL(URL_BASE "/queryOTA_sendsNotif2.json"); // minor = 3
    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(2);
    setFirmwarePatchVersion(0);

    sema = xSemaphoreCreateBinary();

    success = xTaskCreate(vSendsNotifOTAMock, "otaMock", 2000, sema, 10, &otaMockTask);
    TEST_ASSERT_EQUAL(pdPASS, success);

    setOTATask(otaMockTask);
    
    handleActionQueryOTA();

    success = xSemaphoreTake(sema, pdMS_TO_TICKS(1000)); // wait 1 second
    TEST_ASSERT_EQUAL(pdFALSE, success);

    vTaskDelete(otaMockTask);
    vSemaphoreDelete(sema);

    /* test that major updates does not send task notification */
    setUpgradeVersionURL(URL_BASE "/queryOTA_sendsNotif3.json"); // major = 1, minor = 0
    setHardwareVersion(2);
    setHardwareRevision(0);
    setFirmwareMajorVersion(0);
    setFirmwareMinorVersion(2);
    setFirmwarePatchVersion(0);

    sema = xSemaphoreCreateBinary();

    success = xTaskCreate(vSendsNotifOTAMock, "otaMock", 2000, sema, 10, &otaMockTask);
    TEST_ASSERT_EQUAL(pdPASS, success);

    setOTATask(otaMockTask);
    
    handleActionQueryOTA();

    success = xSemaphoreTake(sema, pdMS_TO_TICKS(1000)); // wait 1 second
    TEST_ASSERT_EQUAL(pdFALSE, success);

    vTaskDelete(otaMockTask);
    vSemaphoreDelete(sema);
}