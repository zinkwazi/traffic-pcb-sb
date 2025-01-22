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

TEST_CASE("addCommandToI2CQueue", "[dots_commands]")
{
    const int I2CQueueSize = 20; // 20 elements
    TEST_ASSERT_GREATER_THAN(0, I2CQueueSize);
    I2CCommand command;
    QueueHandle_t I2CQueue = xQueueCreate(I2CQueueSize, sizeof(I2CCommand));
    TEST_ASSERT_NOT_EQUAL(NULL, I2CQueue);

    TaskHandle_t gatekeeperHandle;
    TEST_ASSERT_EQUAL(ESP_OK, createI2CGatekeeperTask(&gatekeeperHandle, I2CQueue));
    int gatekeeperPrio = uxTaskPriorityGet(gatekeeperHandle);

    /* test NULL queue */
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio);
    xQueueReset(I2CQueue);
    TEST_ASSERT_EQUAL(ESP_FAIL, addCommandToI2CQueue(NULL, NOTIFY_OK_VAL, NULL, NULL, DOTS_ASYNC));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1));
    TEST_ASSERT_EQUAL(ESP_FAIL, addCommandToI2CQueue(NULL, NOTIFY_OK_VAL, NULL, xTaskGetCurrentTaskHandle(), DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1));

    /* test invalid I2CCommandFunc */
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio);
    xQueueReset(I2CQueue);
    TEST_ASSERT_EQUAL(ESP_FAIL, addCommandToI2CQueue(I2CQueue, INT_MAX, NULL, NULL, DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1)); // no command should've been added
    TEST_ASSERT_EQUAL(ESP_FAIL, addCommandToI2CQueue(I2CQueue, INT_MAX, NULL, xTaskGetCurrentTaskHandle(), DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1));
    TEST_ASSERT_EQUAL(ESP_FAIL, addCommandToI2CQueue(I2CQueue, INT_MAX, NULL, NULL, DOTS_ASYNC));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1));
    TEST_ASSERT_EQUAL(ESP_FAIL, addCommandToI2CQueue(I2CQueue, INT_MAX, NULL, xTaskGetCurrentTaskHandle(), DOTS_ASYNC));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1));

    /* test receives notification from gatekeeper */
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio);
    xQueueReset(I2CQueue);
    TEST_ASSERT_EQUAL(ESP_OK, addCommandToI2CQueue(I2CQueue, NOTIFY_OK_VAL, NULL, xTaskGetCurrentTaskHandle(), DOTS_ASYNC));
    TEST_ASSERT_EQUAL(DOTS_OK_VAL, ulTaskNotifyTake(pdTRUE, 1));
}

TEST_CASE("dotsSetOperatingMode", "[dots_commands]")
{
    const uint8_t configPage = 4;
    const uint8_t configRegAddr = 0x00;
    const uint8_t softwareShutdownBits = 0x01;
    const int I2CQueueSize = 20; // 20 elements
    TEST_ASSERT_GREATER_THAN(0, I2CQueueSize);
    PageState state;
    MatrixHandles matrices;
    I2CCommand command;
    TaskHandle_t gatekeeperHandle;
    uint8_t result1, result2, result3;

    QueueHandle_t I2CQueue = xQueueCreate(I2CQueueSize, sizeof(I2CCommand));
    TEST_ASSERT_NOT_EQUAL(NULL, I2CQueue);
    TEST_ASSERT_EQUAL(ESP_OK, createI2CGatekeeperTask(&gatekeeperHandle, I2CQueue));
    int gatekeeperPrio = uxTaskPriorityGet(gatekeeperHandle);

    /* test NULL queue */
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio);
    xQueueReset(I2CQueue);
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(NULL, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING));
    
    /* test invalid operation */
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio);
    xQueueReset(I2CQueue);
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(I2CQueue, INT_MAX, DOTS_NOTIFY, DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1)); // function should have put nothing on queue
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(I2CQueue, INT_MAX, DOTS_SILENT, DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1));
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(I2CQueue, INT_MAX, DOTS_NOTIFY, DOTS_ASYNC));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1));
    TEST_ASSERT_EQUAL(ESP_FAIL, dotsSetOperatingMode(I2CQueue, INT_MAX, DOTS_SILENT, DOTS_ASYNC));
    TEST_ASSERT_EQUAL(pdFALSE, xQueuePeek(I2CQueue, &command, 1));

    /* test async mechanism, ie. gatekeeper sends task notification */
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio);
    xQueueReset(I2CQueue);
    TEST_ASSERT_EQUAL(ESP_OK, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_ASYNC));
    TEST_ASSERT_EQUAL(DOTS_OK_VAL, ulTaskNotifyTake(pdTRUE, 1));

    /* test that blocking mechanism retrieves task notification */
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio);
    xQueueReset(I2CQueue);
    xTaskNotifyGive(xTaskGetCurrentTaskHandle()); // function should retrieve 1 and fail
    TEST_ASSERT_EQUAL(DOTS_ERR_VAL, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING));
    TEST_ASSERT_EQUAL(DOTS_OK_VAL, ulTaskNotifyTake(pdTRUE, 1));

    /* test blocking mechanism */
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio);
    xQueueReset(I2CQueue);
    TEST_ASSERT_EQUAL(ESP_OK, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING));

    /* test silent operation */    
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio + 2); // if gatekeeper prio is 0, then setting this
                                                            // tasks prio to -1 would break the test
    xQueueReset(I2CQueue);
    xTaskNotifyGive(xTaskGetCurrentTaskHandle());
    xTaskNotifyGive(xTaskGetCurrentTaskHandle());
    TEST_ASSERT_EQUAL(ESP_OK, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_SILENT, DOTS_ASYNC));
    TEST_ASSERT_EQUAL(2, ulTaskNotifyTake(pdTRUE, 1)); // expect that gatekeeper did not override task notification

    /* Test operation changes the expected registers */
    vTaskPrioritySet(NULL, gatekeeperPrio + 1);
    vTaskPrioritySet(gatekeeperHandle, gatekeeperPrio);
    xQueueReset(I2CQueue);
    TEST_ASSERT_EQUAL(ESP_OK, dInitializeBus(&state, &matrices, I2C_PORT, SDA_PIN, SCL_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, dSetRegisters(&state, matrices, configPage, configRegAddr, SOFTWARE_SHUTDOWN));
    TEST_ASSERT_EQUAL(ESP_OK, dGetRegisters(&result1, &result2, &result3, &state, matrices, configPage, configRegAddr));
    TEST_ASSERT_EQUAL(softwareShutdownBits, ~result1 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, ~result2 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, ~result3 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(ESP_OK, dReleaseBus(&matrices));
    TEST_ASSERT_EQUAL(ESP_OK, dotsReaquireBus(I2CQueue, DOTS_NOTIFY, DOTS_BLOCKING));
    ESP_LOGI(TAG, "setting operating mode");
    TEST_ASSERT_EQUAL(ESP_OK, dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING));
    ESP_LOGI(TAG, "gatekeeper releasing bus");
    TEST_ASSERT_EQUAL(ESP_OK, dotsReleaseBus(I2CQueue, DOTS_NOTIFY, DOTS_BLOCKING));
    ESP_LOGI(TAG, "initializing bus");
    TEST_ASSERT_EQUAL(ESP_OK, dInitializeBus(&state, &matrices, I2C_PORT, SDA_PIN, SCL_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, dGetRegisters(&result1, &result2, &result3, &state, matrices, configPage, configRegAddr));
    TEST_ASSERT_EQUAL(softwareShutdownBits, result1 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, result2 & softwareShutdownBits);
    TEST_ASSERT_EQUAL(softwareShutdownBits, result3 & softwareShutdownBits);
}