

#include "verifier.h"
#include "pinout.h"
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include "unity.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#define TAG "verifier"

void toggleButtonISR(void *params) {
    static struct timeval prevTime = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    struct timeval currTime;
    SemaphoreHandle_t sema = ((VerificationResources*) params)->sema;
    bool *correct = ((VerificationResources*) params)->correct;
    bool *waiting = ((VerificationResources*) params)->waiting;

    if (!(*waiting)) {
        return;
    }

    gettimeofday(&currTime, NULL);
    if ((currTime.tv_sec - prevTime.tv_sec) * 1000000 + (currTime.tv_usec - prevTime.tv_usec) < 250000)
    {
        return;
    } else
    {
        prevTime.tv_sec = currTime.tv_sec;
        prevTime.tv_usec = currTime.tv_usec;
    }

    *waiting = false;
    *correct = true;
    BaseType_t higherPrioTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sema, &higherPrioTaskWoken);
    portYIELD_FROM_ISR(higherPrioTaskWoken);
}

void OTAButtonISR(void *params) {
    static struct timeval prevTime = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    struct timeval currTime;
    SemaphoreHandle_t sema = ((VerificationResources*) params)->sema;
    bool *correct = ((VerificationResources*) params)->correct;
    bool *waiting = ((VerificationResources*) params)->waiting;

    if (!(*waiting)) {
        return;
    }

    gettimeofday(&currTime, NULL);
    if ((currTime.tv_sec - prevTime.tv_sec) * 1000000 + (currTime.tv_usec - prevTime.tv_usec) < 250000)
    {
        return;
    } else
    {
        prevTime.tv_sec = currTime.tv_sec;
        prevTime.tv_usec = currTime.tv_usec;
    }

    *waiting = false;
    *correct = false;
    BaseType_t higherPrioTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sema, &higherPrioTaskWoken);
    portYIELD_FROM_ISR(higherPrioTaskWoken);
}

/**
 * @brief Installs the buttonISR onto the toggle button, which requires
 *        T_SW_PIN to be defined properly in the pinout.
 * 
 * @param[out] out_sema The function will place the location of a semaphore
 *        which is upped once one of the buttons has been pressed.
 * @param[in] correct A boolean that, once the program unblocks on out_sema,
 *        corresponds to whether the toggle button or OTA button has been pressed.
 *        If the toggle button is pressed, then correct is true. If the OTA button
 *        is pressed, then correct is false.
 * @param[in] waiting A boolean that will be passed to the button interrupts to
 *        tell them whether the button press is valid, ie. the program is waiting
 *        for one of the two button presses. After setting correct, the ISRs will
 *        turn waiting low so that multiple button presses per verifier query are
 *        not registered. The program should make this boolean true before blocking
 *        on out_sema to wait for a button press, otherwise it will block indefinitely.
 * 
 * @returns ESP_OK if initialization was successful, otherwise ESP_FAIL.
 */
esp_err_t initializeVerificationButtons(VerificationResources *resources) {
    static VerificationResources buttonParams;
    static bool correct;
    static bool waiting;
    /* input guards */
    TEST_ASSERT_NOT_EQUAL(NULL, resources);
    /* set initial state of resources */
    correct = false;
    waiting = false;
    resources->correct = &correct;
    resources->waiting = &waiting;
    /* package parameters */
    resources->sema = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_EQUAL(NULL, resources->sema);
    buttonParams.sema = resources->sema;
    buttonParams.correct = resources->correct;
    buttonParams.waiting = resources->waiting;
    /* Initialize toggle button */
    TEST_ASSERT_EQUAL(ESP_OK, gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT));
    TEST_ASSERT_EQUAL(ESP_OK, gpio_set_intr_type(T_SW_PIN, GPIO_INTR_NEGEDGE));
    TEST_ASSERT_EQUAL(ESP_OK, gpio_isr_handler_add(T_SW_PIN, toggleButtonISR, &buttonParams));
    /* Initialize ota button */
    TEST_ASSERT_EQUAL(ESP_OK, gpio_set_direction(IO_SW_PIN, GPIO_MODE_INPUT));
    TEST_ASSERT_EQUAL(ESP_OK, gpio_set_intr_type(IO_SW_PIN, GPIO_INTR_NEGEDGE));
    TEST_ASSERT_EQUAL(ESP_OK, gpio_isr_handler_add(IO_SW_PIN, OTAButtonISR, &buttonParams));
    return ESP_OK;
}

/**
 * @brief Prompts the human verifier to press either the "Toggle" or "OTA" button
 *        to verify the message is true or false, respectively.
 * 
 * @param[in] message The message that is presented to the verifier via the default
 *        logging interface.
 * @param[in] expected Whether message is expected to be true or false, ie. whether
 *        a toggle button press is expected or an OTA button press.
 * @param[in] res Resources that are shared between this program and the button
 *        interrupts to enable communication across them.
 */
void assertHumanVerifies(char *message, bool expected, VerificationResources res) {
    ESP_LOGI(TAG, "%s", message);
    *(res.waiting) = true;
    while (xSemaphoreTake(res.sema, INT_MAX) != pdTRUE) {}
    TEST_ASSERT_NOT_EQUAL(true, *(res.waiting));
    TEST_ASSERT_EQUAL(expected, *(res.correct));
}