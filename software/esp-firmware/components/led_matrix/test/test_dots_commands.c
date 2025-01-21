#include "unity.h"
#include "dots_commands.h"
#include "pinout.h"
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"

#define TAG "test"

/**
 * Tests that task notifications are sent in both blocking and async
 * mode with the dotsSetOperatingMode function and not sent when notify
 * is false.
 */
TEST_CASE("dotsSetOperatingMode gatekeeperNotify", "[dots_commands]")
{
    const int I2CQueueSize = 20; // 20 elements
    TEST_ASSERT_GREATER_THAN(0, I2CQueueSize);
    QueueHandle_t I2CQueue = xQueueCreate(I2CQueueSize, sizeof(I2CCommand));
    TEST_ASSERT_NOT_EQUAL(NULL, I2CQueue);

    TaskHandle_t gatekeeperHandle;
    TEST_ASSERT_EQUAL(ESP_OK, createI2CGatekeeperTask(&gatekeeperHandle, I2CQueue));
    int gatekeeperPrio = uxTaskPriorityGet(gatekeeperHandle);
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);

    /* test async mechanism, ie. gatekeeper actually sends notification */
    TEST_ASSERT_EQUAL(ESP_OK, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_ASYNC));
    while (ulTaskNotifyTake(pdTRUE, INT_MAX) == 0) {} // will timeout the test

    /* test that blocking mechanism actually retrieves task notification
       by setting the wrong value to retrieve */
    xTaskNotifyGive(xTaskGetCurrentTaskHandle()); // dotsReset should retrieve 1 and fail
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING));
    /* now expect to retrieve true gatekeeper value */
    uint32_t returnValue = 0;
    while ((returnValue = ulTaskNotifyTake(pdTRUE, INT_MAX)) == 0) {} // will timeout the test

    /* test blocking mechanism */
    TEST_ASSERT_EQUAL(ESP_OK, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING));

    /* test silent operation */
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio + 2); // if gatekeeper prio is 0, then setting this
                                                            // tasks prio to -1 would break the test
    xTaskNotifyGive(xTaskGetCurrentTaskHandle());
    xTaskNotifyGive(xTaskGetCurrentTaskHandle());
    TEST_ASSERT_EQUAL(ESP_OK, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_SILENT, DOTS_ASYNC));
    while ((returnValue = ulTaskNotifyTake(pdTRUE, INT_MAX)) == 0) {}
    TEST_ASSERT_EQUAL(2, returnValue); // expect that gatekeeper did not override task notification
}

/**
 * Test that the operation changes the expected registers.
 */
TEST_CASE("dotsSetOperatingMode changesRegisters", "[dots_commands]")
{
    const uint8_t configPage = 4;
    const uint8_t configRegAddr = 0x00;
    const uint8_t softwareShutdownBits = 0x01;
    const int I2CQueueSize = 20; // 20 elements
    TEST_ASSERT_GREATER_THAN(0, I2CQueueSize);
    QueueHandle_t I2CQueue = xQueueCreate(I2CQueueSize, sizeof(I2CCommand));
    TEST_ASSERT_NOT_EQUAL(NULL, I2CQueue);

    /* Set matrices to software shutdown */
    PageState state;
    MatrixHandles matrices;
    uint8_t result1, result2, result3;
    TEST_ASSERT_EQUAL(ESP_OK, dInitializeBus(&state, &matrices, I2C_PORT, SDA_PIN, SCL_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, dSetRegisters(&state, matrices, configPage, configRegAddr, SOFTWARE_SHUTDOWN));
    TEST_ASSERT_EQUAL(ESP_OK, dGetRegisters(&result1, &result2, &result3, &state, matrices, configPage, configRegAddr));
    TEST_ASSERT_EQUAL(softwareShutdownBits, ~result1 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, ~result2 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, ~result3 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(ESP_OK, dReleaseBus(matrices));

    /* Check that dotsSetOperatingMode changes software shutdown bits */
    TaskHandle_t gatekeeperHandle;
    TEST_ASSERT_EQUAL(ESP_OK, createI2CGatekeeperTask(&gatekeeperHandle, I2CQueue));
    int gatekeeperPrio = uxTaskPriorityGet(gatekeeperHandle);
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    
    TEST_ASSERT_EQUAL(ESP_OK, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(ESP_OK, dotsReleaseBus(I2CQueue, DOTS_NOTIFY, DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(ESP_OK, dInitializeBus(&state, &matrices, I2C_PORT, SDA_PIN, SCL_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, dGetRegisters(&result1, &result2, &result3, &state, matrices, configPage, configRegAddr));
    TEST_ASSERT_EQUAL(softwareShutdownBits, result1 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, result2 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, result3 & softwareShutdownBits);
}

TEST_CASE("dotsSetOperatingMode inputGuards", "[dots_commands]")
{
    const int I2CQueueSize = 20; // 20 elements
    TEST_ASSERT_GREATER_THAN(0, I2CQueueSize);
    QueueHandle_t I2CQueue = xQueueCreate(I2CQueueSize, sizeof(I2CCommand));
    TEST_ASSERT_NOT_EQUAL(NULL, I2CQueue);
    TaskHandle_t gatekeeperHandle;
    TEST_ASSERT_EQUAL(ESP_OK, createI2CGatekeeperTask(&gatekeeperHandle, I2CQueue));
    int gatekeeperPrio = uxTaskPriorityGet(gatekeeperHandle);
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);

    /* test NULL queue */
    ESP_LOGI(TAG, "testing NULL queue handle");
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(NULL, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING));
    
    /* test invalid operation */
    ESP_LOGI(TAG, "testing INT_MAX operation");
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(I2CQueue, INT_MAX, DOTS_NOTIFY, DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(I2CQueue, INT_MAX, DOTS_SILENT, DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(I2CQueue, INT_MAX, DOTS_NOTIFY, DOTS_ASYNC));
    while (ulTaskNotifyTake(pdTRUE, INT_MAX) == 0) {} // will timeout the test
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(I2CQueue, INT_MAX, DOTS_SILENT, DOTS_ASYNC));
    while (ulTaskNotifyTake(pdTRUE, INT_MAX) == 0) {} // will timeout the test
}

