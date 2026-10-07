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

#include "input_queue.h"

#define TEST_GROUP "[input]"

/**
 * @brief Ensures the main input queue exists, independent of test
 * execution order. There is no deinit function, so once initialized the
 * queue stays initialized for the rest of the test binary.
 */
static void ensureInputQueueInitialized(void)
{
    if (NULL == mainInputQueue)
    {
        TEST_ASSERT_EQUAL(ESP_OK, initInputQueue());
    }
}

TEST_CASE("initInput_rejectsNullOTATask", TEST_GROUP)
{
    ensureInputQueueInitialized();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, initInput(NULL));
}

TEST_CASE("initInput_rejectsUninitializedInputQueue", TEST_GROUP)
{
    ensureInputQueueInitialized();

    /* there is no way to deinit the queue, so hide it for the duration of the test */
    QueueHandle_t savedQueue = mainInputQueue;
    mainInputQueue = NULL;
    esp_err_t err = initInput(xTaskGetCurrentTaskHandle());
    mainInputQueue = savedQueue;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, err);
}

TEST_CASE("initInput_succeeds", TEST_GROUP)
{
    ensureInputQueueInitialized();

    /* initInput requires the GPIO ISR service; it may already be installed */
    esp_err_t err = gpio_install_isr_service(0);
    TEST_ASSERT_TRUE(err == ESP_OK || err == ESP_ERR_INVALID_STATE);

    TEST_ASSERT_EQUAL(ESP_OK, initInput(xTaskGetCurrentTaskHandle()));
}
