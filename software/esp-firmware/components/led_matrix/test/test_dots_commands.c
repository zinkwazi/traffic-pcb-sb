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
    ESP_LOGI(TAG, "Manually setting matrices into software shutdown mode");
    PageState state;
    MatrixHandles matrices;
    uint8_t result1, result2, result3;
    TEST_ASSERT_EQUAL(ESP_OK, dInitializeBus(&state, &matrices, I2C_PORT, SDA_PIN, SCL_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, dSetRegisters(&state, matrices, configPage, configRegAddr, SOFTWARE_SHUTDOWN));
    TEST_ASSERT_EQUAL(ESP_OK, dGetRegisters(&result1, &result2, &result3, &state, matrices, configPage, configRegAddr));
    TEST_ASSERT_EQUAL(softwareShutdownBits, ~result1 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, ~result2 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, ~result3 & softwareShutdownBits);
    ESP_LOGI(TAG, "Releasing bus");
    TEST_ASSERT_EQUAL(ESP_OK, dReleaseBus(matrices));

    ESP_LOGI(TAG, "Creating Gatekeeper");
    TaskHandle_t gatekeeperHandle;
    TEST_ASSERT_EQUAL(ESP_OK, createI2CGatekeeperTask(&gatekeeperHandle, I2CQueue));
    int gatekeeperPrio = uxTaskPriorityGet(gatekeeperHandle);
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    
    ESP_LOGI(TAG, "Calling dotsSetOperatingMode with NORMAL_OPERATION");
    TEST_ASSERT_EQUAL(ESP_OK, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING));
    ESP_LOGI(TAG, "Releasing bus");
    TEST_ASSERT_EQUAL(ESP_OK, dotsReleaseBus(I2CQueue, DOTS_NOTIFY, DOTS_BLOCKING));
    ESP_LOGI(TAG, "Manually checking matrix registers");
    TEST_ASSERT_EQUAL(ESP_OK, dInitializeBus(&state, &matrices, I2C_PORT, SDA_PIN, SCL_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, dGetRegisters(&result1, &result2, &result3, &state, matrices, configPage, configRegAddr));
    TEST_ASSERT_EQUAL(softwareShutdownBits, result1 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, result2 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, result3 & softwareShutdownBits);
}

