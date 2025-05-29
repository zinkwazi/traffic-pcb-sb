/**
 * test_action_queryOTA.c
 * 
 * Unit tests for actions.c:handleActionQueryOTA.
 * 
 * Test file dependencies: None.
 */

#include "unity.h"

#include "esp_heap_trace.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "actions.h"
#include "actions_pi.h"
#include "utilities.h"
#include "ota_config.h"
#include "mock_esp_http_client.h"
#include "wrap_esp_http_client.h"

#include "resources/queryOTAResources.h"

#if CONFIG_ACTIONS_MAIN == 1
#include "Mockota.h"
#include "Mockindicators.h"
#elif CONFIG_ACTIONS_MAIN == 2
/* none */
#endif

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
 * This app runs typical unit tests that require heavy mocking.
 * 
 * CONFIG_MOCK_HTTP_WRAP=y
 * CONFIG_MOCK_OTA=y
 * CONFIG_MOCK_INDICATORS=y
 * CONFIG_MOCK_APP_ERRORS=y
 * CONFIG_MOCK_REFRESH=y # to avoid util extern macro conflicts
 */
#if CONFIG_ACTIONS_MAIN == 1

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
 * This app runs tests that check for memory leaks, which requires
 * few mocks.
 * 
 * CONFIG_MOCK_HTTP_WRAP=y
 */
#elif CONFIG_ACTIONS_MAIN == 2

static int http_calls = 0;

esp_http_client_handle_t httpInitCallback(const esp_http_client_config_t* config, int cmock_num_calls)
{
    if (http_calls != 0)
    {
        TEST_FAIL_MESSAGE("http client inititalized twice in a row");
    }
    http_calls += 1;

    return esp_http_client_init(config);
}

esp_err_t httpDestroyCallback(esp_http_client_handle_t client, int cmock_num_calls)
{
    if (http_calls == 0)
    {
        TEST_FAIL_MESSAGE("http client destroyed without initialization");
    }
    http_calls -= 1;

    return esp_http_client_cleanup(client);
}

/**
 * @brief Checks that a memory leak is not present within the function.
 */
TEST_CASE("queryOTA_memoryLeak", "[actions]")
{
    MOCK_ENDPOINT(version1);

    esp_err_t err;

    /* setup extern macros */
    macroResetUtils();
    macroResetOTAConfig();

    FIRMWARE_UPGRADE_VERSION_URL = version1.url;

    /* initialize mocks */
    err = mock_esp_http_client_add_endpoint(version1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* perform test */
    TEST_ASSERT_EQUAL(ESP_OK, heap_trace_start(HEAP_TRACE_LEAKS));
    err = handleActionQueryOTA();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    vTaskDelay(pdMS_TO_TICKS(1000));

    TEST_ASSERT_EQUAL(ESP_OK, heap_trace_stop());
    
    /* analyze heap */
    heap_trace_dump();
}

#endif /* CONFIG_ACTIONS_MAIN */
#endif /* CONFIG_HARDWARE_VERSION != 1 */