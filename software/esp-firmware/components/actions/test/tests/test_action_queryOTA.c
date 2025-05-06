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

#include "esp_heap_trace.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "actions.h"
#include "actions_pi.h"
#include "utilities.h"
#include "Mockota.h"
#include "Mockindicators.h"
#include "Mockhttp_wrap.h"

#define URL_BASE CONFIG_ACTIONS_TEST_DATA_SERVER CONFIG_ACTIONS_TEST_DATA_BASE_URL

#if CONFIG_HARDWARE_VERSION != 1 // specification does not include this for V1_X due to memory constraints

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
            break;
        }
    }
    while (true)
    {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 0)
        {
            TEST_FAIL_MESSAGE("OTA Mock task received multiple task notifications!");
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Tests that queryOTA action sends a task notification to the OTA
 * task only if a patch update is available.
 */
TEST_CASE("queryOTA_patchUpdateNotif", "[actions]")
{
    const int testPrio = uxTaskPriorityGet(NULL);
    SemaphoreHandle_t sema;
    TaskHandle_t otaMockTask;
    BaseType_t success;

    /* setup mock functions */
    Mockota_Init();
    bool dummy;
    queryOTAUpdateAvailable_ExpectAndReturn(&dummy, &dummy, ESP_OK);
    queryOTAUpdateAvailable_IgnoreArg_available();
    queryOTAUpdateAvailable_IgnoreArg_patch();
    bool retAvailable = true;
    bool retPatch = true;
    queryOTAUpdateAvailable_ReturnThruPtr_available(&retAvailable);
    queryOTAUpdateAvailable_ReturnThruPtr_patch(&retPatch);

    indicateOTAAvailable_IgnoreAndReturn(ESP_OK);
    indicateOTAUpdate_IgnoreAndReturn(ESP_OK);
    indicateOTASuccess_IgnoreAndReturn(ESP_OK);
    indicateOTAFailure_IgnoreAndReturn(ESP_OK);

    // the vSendsNotifOTAMock is given a higher priority than the test task,
    // so by the time handleActionQueryOTA returns the task should have set
    // the binary mutex and returned to sleep.
    sema = xSemaphoreCreateBinary();

    success = xTaskCreate(vSendsNotifOTAMock, "otaMock", 2000, sema, testPrio + 1, &otaMockTask);
    TEST_ASSERT_EQUAL(pdPASS, success);
    getOTATask_IgnoreAndReturn(otaMockTask);
    
    /* perform test */
    handleActionQueryOTA();

    /* check test results */
    success = xSemaphoreTake(sema, 0);
    TEST_ASSERT_EQUAL(pdTRUE, success);

    Mockota_Verify();
    Mockindicators_Verify();

    /* cleanup test resources */
    Mockota_Destroy();

    vTaskDelete(otaMockTask);
    vSemaphoreDelete(sema);
}

/**
 * @brief Tests that queryOTA action does not send a task notification
 * if a non-patch update is available.
 */
TEST_CASE("queryOTA_updateNotif", "[actions]")
{
    const int testPrio = uxTaskPriorityGet(NULL);
    SemaphoreHandle_t sema;
    TaskHandle_t otaMockTask;
    BaseType_t success;

    /* setup mock functions */
    Mockota_Init();
    bool dummy;
    queryOTAUpdateAvailable_ExpectAndReturn(&dummy, &dummy, ESP_OK);
    queryOTAUpdateAvailable_IgnoreArg_available();
    queryOTAUpdateAvailable_IgnoreArg_patch();
    bool retAvailable = true;
    bool retPatch = false;
    queryOTAUpdateAvailable_ReturnThruPtr_available(&retAvailable);
    queryOTAUpdateAvailable_ReturnThruPtr_patch(&retPatch);

    indicateOTAAvailable_IgnoreAndReturn(ESP_OK);
    indicateOTAUpdate_IgnoreAndReturn(ESP_OK);
    indicateOTASuccess_IgnoreAndReturn(ESP_OK);
    indicateOTAFailure_IgnoreAndReturn(ESP_OK);

    // the vSendsNotifOTAMock is given a higher priority than the test task,
    // so by the time handleActionQueryOTA returns the task should have set
    // the binary mutex and returned to sleep.
    sema = xSemaphoreCreateBinary();

    success = xTaskCreate(vSendsNotifOTAMock, "otaMock", 2000, sema, testPrio + 1, &otaMockTask);
    TEST_ASSERT_EQUAL(pdPASS, success);
    getOTATask_IgnoreAndReturn(otaMockTask);
    
    /* perform test */
    handleActionQueryOTA();

    /* check test results */
    success = xSemaphoreTake(sema, 0);
    TEST_ASSERT_EQUAL(pdFALSE, success);

    Mockota_Verify();
    Mockindicators_Verify();

    /* cleanup test resources */
    Mockota_Destroy();

    vTaskDelete(otaMockTask);
    vSemaphoreDelete(sema);
}

/**
 * @brief Tests that queryOTA action does not send a task notification
 * if no update is available
 */
TEST_CASE("queryOTA_noUpdateNotif", "[actions]")
{
    const int testPrio = uxTaskPriorityGet(NULL);
    SemaphoreHandle_t sema;
    TaskHandle_t otaMockTask;
    BaseType_t success;

    /* setup mock functions */
    Mockota_Init();
    bool dummy;
    queryOTAUpdateAvailable_ExpectAndReturn(&dummy, &dummy, ESP_OK);
    queryOTAUpdateAvailable_IgnoreArg_available();
    queryOTAUpdateAvailable_IgnoreArg_patch();
    bool retAvailable = false;
    bool retPatch = false;
    queryOTAUpdateAvailable_ReturnThruPtr_available(&retAvailable);
    queryOTAUpdateAvailable_ReturnThruPtr_patch(&retPatch);

    indicateOTAAvailable_IgnoreAndReturn(ESP_OK);
    indicateOTAUpdate_IgnoreAndReturn(ESP_OK);
    indicateOTASuccess_IgnoreAndReturn(ESP_OK);
    indicateOTAFailure_IgnoreAndReturn(ESP_OK);

    // the vSendsNotifOTAMock is given a higher priority than the test task,
    // so by the time handleActionQueryOTA returns the task should have set
    // the binary mutex and returned to sleep.
    sema = xSemaphoreCreateBinary();

    success = xTaskCreate(vSendsNotifOTAMock, "otaMock", 2000, sema, testPrio + 1, &otaMockTask);
    TEST_ASSERT_EQUAL(pdPASS, success);
    getOTATask_IgnoreAndReturn(otaMockTask);
    
    /* perform test */
    handleActionQueryOTA();

    /* check test results */
    success = xSemaphoreTake(sema, 0);
    TEST_ASSERT_EQUAL(pdFALSE, success);

    Mockota_Verify();
    Mockindicators_Verify();

    /* cleanup test resources */
    Mockota_Destroy();

    vTaskDelete(otaMockTask);
    vSemaphoreDelete(sema);
}


/**
 * @brief Checks that a memory leak is not present within the function.
 */
TEST_CASE("queryOTA_memoryLeak", "[actions]")
{
    // TODO: This test does not check the actual heap because dependencies are
    // mocked.

    const int testPrio = uxTaskPriorityGet(NULL);
    SemaphoreHandle_t sema;
    TaskHandle_t otaMockTask;
    BaseType_t success;

    /* setup mock functions */
    Mockota_Init();
    bool dummy;
    queryOTAUpdateAvailable_ExpectAndReturn(&dummy, &dummy, ESP_OK);
    queryOTAUpdateAvailable_IgnoreArg_available();
    queryOTAUpdateAvailable_IgnoreArg_patch();
    bool retAvailable = false;
    bool retPatch = false;
    queryOTAUpdateAvailable_ReturnThruPtr_available(&retAvailable);
    queryOTAUpdateAvailable_ReturnThruPtr_patch(&retPatch);

    indicateOTAAvailable_IgnoreAndReturn(ESP_OK);
    indicateOTAUpdate_IgnoreAndReturn(ESP_OK);
    indicateOTASuccess_IgnoreAndReturn(ESP_OK);
    indicateOTAFailure_IgnoreAndReturn(ESP_OK);

    // the vSendsNotifOTAMock is given a higher priority than the test task,
    // so by the time handleActionQueryOTA returns the task should have set
    // the binary mutex and returned to sleep.
    sema = xSemaphoreCreateBinary();

    success = xTaskCreate(vSendsNotifOTAMock, "otaMock", 2000, sema, testPrio + 1, &otaMockTask);
    TEST_ASSERT_EQUAL(pdPASS, success);
    getOTATask_IgnoreAndReturn(otaMockTask);
    
    /* perform test */
    TEST_ASSERT_EQUAL(ESP_OK, heap_trace_start(HEAP_TRACE_LEAKS));
    handleActionQueryOTA();
    TEST_ASSERT_EQUAL(ESP_OK, heap_trace_stop());
    
    /* analyze heap */
    heap_trace_dump();

    /* cleanup test resources */
    Mockota_Destroy();

    vTaskDelete(otaMockTask);
    vSemaphoreDelete(sema);

    
}

#endif /* CONFIG_HARDWARE_VERSION != 1 */