/**
 * button_debounce_fsm.c
 * 
 * Created On: 9/6/2026
 * Author: Jaden Baptista
 * 
 * Contains a timed automata that implements button debouncing,
 * with timing handled externally.
 */

#include "button_debounce_fsm.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

/**
 * Initializes a button debounce FSM.
 *
 * @param button The button debounce FSM to initialize.
 *
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 */
esp_err_t initButtonDebounceFSM(ButtonDebounceState *button)
{
    if (NULL == button) return ESP_ERR_INVALID_ARG;
    *button = BUTTON_VERIFYING_INIT;
    return ESP_OK;
}

/**
 * An FSM that handles button debouncing.
 * 
 * @note This FSM is not thread safe. Calls on the same button
 * must be serialized, such as by calling it from a single task.
 * 
 * @param button The buttom FSM.
 * @param buttonHigh Whether the GPIO pin is high or low at the
 * time of the interrupt.
 * @param edgeDetected Whether the reason for calling the FSM is
 * due to an edge that was detected on the GPIO pin. Otherwise,
 * the FSM should only be called if the timer it requested expired.
 * 
 * @returns The output of the FSM.
 */
ButtonDebounceFSMOutput buttonDebounceFSM(ButtonDebounceState *button, bool buttonHigh, bool edgeDetected)
{
    ButtonDebounceFSMOutput out = {
        .state = BUTTON_LEVEL_UNKNOWN,
        .monitorEdges = true,
        .debounceTimeUs = 0,
    };

    switch (*button)
    {
        case BUTTON_LOW:
            if (edgeDetected)
            {
                *button = BUTTON_DEBOUNCING_LOW_TO_HIGH;
                out.monitorEdges = false;
                out.debounceTimeUs = CONFIG_BUTTON_DEBOUNCE_US;
            } else
            {
                out.monitorEdges = true;
                out.debounceTimeUs = 0;
            }
            out.state = BUTTON_LEVEL_LOW;
            return out;
        case BUTTON_HIGH:
            if (edgeDetected)
            {
                *button = BUTTON_DEBOUNCING_HIGH_TO_LOW;
                out.monitorEdges = false;
                out.debounceTimeUs = CONFIG_BUTTON_DEBOUNCE_US;
            } else
            {
                out.monitorEdges = true;
                out.debounceTimeUs = 0;
            }
            out.state = BUTTON_LEVEL_HIGH;
            return out;
        case BUTTON_DEBOUNCING_INIT:
            *button = BUTTON_VERIFYING_INIT;
            out.monitorEdges = true;
            out.debounceTimeUs = CONFIG_BUTTON_VERIFY_US;
            out.state = BUTTON_LEVEL_UNKNOWN;
            return out;
        case BUTTON_DEBOUNCING_LOW_TO_HIGH:
            *button = BUTTON_VERIFYING_LOW_TO_HIGH;
            out.monitorEdges = true;
            out.debounceTimeUs = CONFIG_BUTTON_VERIFY_US;
            out.state = BUTTON_LEVEL_LOW;
            return out;
        case BUTTON_DEBOUNCING_HIGH_TO_LOW:
            *button = BUTTON_VERIFYING_HIGH_TO_LOW;
            out.monitorEdges = true;
            out.debounceTimeUs = CONFIG_BUTTON_VERIFY_US;
            out.state = BUTTON_LEVEL_HIGH;
            return out;
        case BUTTON_VERIFYING_INIT:
            if (edgeDetected)
            {
                *button = BUTTON_DEBOUNCING_INIT;
                out.state = BUTTON_LEVEL_UNKNOWN;
                out.monitorEdges = false;
                out.debounceTimeUs = CONFIG_BUTTON_DEBOUNCE_US;
            } else
            {
                if (buttonHigh)
                {
                    *button = BUTTON_HIGH;
                    out.state = BUTTON_LEVEL_HIGH;
                } else
                {
                    *button = BUTTON_LOW;
                    out.state = BUTTON_LEVEL_LOW;
                }
                out.monitorEdges = true;
                out.debounceTimeUs = 0;
            }
            return out;
        case BUTTON_VERIFYING_LOW_TO_HIGH:
            if (edgeDetected)
            {
                *button = BUTTON_DEBOUNCING_LOW_TO_HIGH;
                out.state = BUTTON_LEVEL_LOW;
                out.monitorEdges = false;
                out.debounceTimeUs = CONFIG_BUTTON_DEBOUNCE_US;
            } else
            {
                if (buttonHigh)
                {
                    *button = BUTTON_HIGH;
                    out.state = BUTTON_EDGE_LOW_TO_HIGH;
                } else
                {
                    *button = BUTTON_LOW;
                    out.state = BUTTON_LEVEL_LOW;
                }
                out.monitorEdges = true;
                out.debounceTimeUs = 0;
            }
            return out;
        case BUTTON_VERIFYING_HIGH_TO_LOW:
            if (edgeDetected)
            {
                *button = BUTTON_DEBOUNCING_HIGH_TO_LOW;
                out.state = BUTTON_LEVEL_HIGH;
                out.monitorEdges = false;
                out.debounceTimeUs = CONFIG_BUTTON_DEBOUNCE_US;
            } else
            {
                if (buttonHigh)
                {
                    *button = BUTTON_HIGH;
                    out.state = BUTTON_LEVEL_HIGH;
                } else
                {
                    *button = BUTTON_LOW;
                    out.state = BUTTON_EDGE_HIGH_TO_LOW;
                }
                out.monitorEdges = true;
                out.debounceTimeUs = 0;
            }
            return out;
    }

    /* should never be here. Reinitialize button state */
    *button = BUTTON_VERIFYING_INIT;
    out.state = BUTTON_LEVEL_UNKNOWN;
    out.monitorEdges = true;
    out.debounceTimeUs = CONFIG_BUTTON_VERIFY_US;
    return out;
}
