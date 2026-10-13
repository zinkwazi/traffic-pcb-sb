/**
 * test_input.c
 *
 * Unit tests for input.h. These ensure that fakes match real hardware behavior.
 *
 * @note These run against the input fake (CONFIG_FAKE_INPUT) in the
 * test_refresh configuration.
 */

#include "input.h"

#include <stddef.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "unity.h"

#define TEST_GROUP "[input]"

/**
 * @brief Ensures input is initialized, independent of test execution
 * order. There is no deinit function, so once initialized the input
 * component stays initialized for the rest of the test binary.
 */
static void ensureInputInitialized(void)
{
    if (NULL == mainInputQueue)
    {
        /* initInput requires the GPIO ISR service; it may already be installed */
        esp_err_t err = gpio_install_isr_service(0);
        TEST_ASSERT_TRUE(err == ESP_OK || err == ESP_ERR_INVALID_STATE);

        TEST_ASSERT_EQUAL(ESP_OK, initInput(xTaskGetCurrentTaskHandle()));
    }
}

/**
 * @brief Drains the abort count so each abort count test starts from zero.
 */
static void resetAbortCount(void)
{
    while (!abortCountZero())
    {
        TEST_ASSERT_EQUAL(ESP_OK, decrementAbortCount());
    }
}

TEST_CASE("initInput_rejectsNullOTATask", TEST_GROUP)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, initInput(NULL));
}

TEST_CASE("initInput_createsMainInputQueue", TEST_GROUP)
{
    ensureInputInitialized();

    TEST_ASSERT_NOT_NULL(mainInputQueue);
}

TEST_CASE("mainInputQueue_holdsMainCommands", TEST_GROUP)
{
    MainCommand sent = MAIN_CMD_SCHEDULED;
    MainCommand received = MAIN_CMD_ERROR;

    ensureInputInitialized();
    xQueueReset(mainInputQueue);

    TEST_ASSERT_EQUAL(pdTRUE, xQueueSend(mainInputQueue, &sent, 0));
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(mainInputQueue, &received, 0));
    TEST_ASSERT_EQUAL(sent, received);
}

TEST_CASE("abortCount_incrementMakesNonZero", TEST_GROUP)
{
    ensureInputInitialized();
    resetAbortCount();

    TEST_ASSERT_TRUE(abortCountZero());
    TEST_ASSERT_EQUAL(ESP_OK, incrementAbortCount());
    TEST_ASSERT_FALSE(abortCountZero());

    resetAbortCount();
}

TEST_CASE("abortCount_decrementReturnsToZero", TEST_GROUP)
{
    ensureInputInitialized();
    resetAbortCount();

    TEST_ASSERT_EQUAL(ESP_OK, incrementAbortCount());
    TEST_ASSERT_EQUAL(ESP_OK, incrementAbortCount());
    TEST_ASSERT_EQUAL(ESP_OK, decrementAbortCount());
    TEST_ASSERT_FALSE(abortCountZero());
    TEST_ASSERT_EQUAL(ESP_OK, decrementAbortCount());
    TEST_ASSERT_TRUE(abortCountZero());
}

TEST_CASE("abortCount_decrementAtZeroFails", TEST_GROUP)
{
    ensureInputInitialized();
    resetAbortCount();

    TEST_ASSERT_EQUAL(ESP_FAIL, decrementAbortCount());
    TEST_ASSERT_TRUE(abortCountZero());
}
