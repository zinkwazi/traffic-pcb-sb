/**
 * button_debounce_fsm.h
 * 
 * Created On: 9/6/2026
 * Author: Jaden Baptista
 * 
 * Contains a timed automata that implements button debouncing,
 * with timing handled externally.
 */

#ifndef INC_BUTTON_DEBOUNCE_FSM_H_
#define INC_BUTTON_DEBOUNCE_FSM_H_

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"

typedef enum ButtonDebounceState {
    BUTTON_LOW,
    BUTTON_HIGH,
    BUTTON_DEBOUNCING_INIT,
    BUTTON_DEBOUNCING_LOW_TO_HIGH,
    BUTTON_DEBOUNCING_HIGH_TO_LOW,
    BUTTON_VERIFYING_INIT,
    BUTTON_VERIFYING_LOW_TO_HIGH,
    BUTTON_VERIFYING_HIGH_TO_LOW,
} ButtonDebounceState;

typedef enum ButtonState {
    BUTTON_LEVEL_UNKNOWN,
    BUTTON_LEVEL_LOW,
    BUTTON_LEVEL_HIGH,
    BUTTON_EDGE_LOW_TO_HIGH,
    BUTTON_EDGE_HIGH_TO_LOW,
} ButtonState;

typedef struct ButtonDebounceFSMOutput {
    /* The debounced button output of the FSM. */
    ButtonState state;
    /* the caller should monitor edges and call the FSM if an edge occurs. */
    bool monitorEdges;
    /* The amount of time in microseconds to set a timer for before calling the FSM again. The timer should be ended early
     * and the FSM called again if monitorEdges is true and an edge was detected. */
    uint32_t debounceTimeUs;
} ButtonDebounceFSMOutput;

esp_err_t initButtonDebounceFSM(ButtonDebounceState *button);
ButtonDebounceFSMOutput buttonDebounceFSM(ButtonDebounceState *button, bool buttonHigh, bool edgeDetected);

#endif /* INC_BUTTON_DEBOUNCE_FSM_H_ */