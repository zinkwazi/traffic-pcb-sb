/**
 * test_button_debounce_fsm.c
 *
 * Unit tests for button_debounce_fsm.h.
 */

#include "button_debounce_fsm.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "unity.h"
#include "unity_test_runner.h"

#define TEST_GROUP "button_debounce_fsm"

static ButtonDebounceFSMOutput tick(ButtonDebounceState *button, bool buttonHigh, bool edgeDetected)
{
    return buttonDebounceFSM(button, buttonHigh, edgeDetected);
}

/* Drives the FSM from a freshly-initialized state to LOW, with no edges seen. */
static void settleInitToLow(ButtonDebounceState *button)
{
    TEST_ASSERT_EQUAL(ESP_OK, initButtonDebounceFSM(button));
    tick(button, false, false);
    TEST_ASSERT_EQUAL(BUTTON_LOW, *button);
}

/* Drives the FSM from a freshly-initialized state to HIGH, with no edges seen. */
static void settleInitToHigh(ButtonDebounceState *button)
{
    TEST_ASSERT_EQUAL(ESP_OK, initButtonDebounceFSM(button));
    tick(button, true, false);
    TEST_ASSERT_EQUAL(BUTTON_HIGH, *button);
}

/* From LOW, drives an edge through debounce into VERIFYING_LOW_TO_HIGH. */
static void enterVerifyingLowToHigh(ButtonDebounceState *button)
{
    tick(button, true, true); /* LOW -> DEBOUNCING_LOW_TO_HIGH */
    tick(button, true, false); /* DEBOUNCING_LOW_TO_HIGH -> VERIFYING_LOW_TO_HIGH */
    TEST_ASSERT_EQUAL(BUTTON_VERIFYING_LOW_TO_HIGH, *button);
}

/* From HIGH, drives an edge through debounce into VERIFYING_HIGH_TO_LOW. */
static void enterVerifyingHighToLow(ButtonDebounceState *button)
{
    tick(button, false, true); /* HIGH -> DEBOUNCING_HIGH_TO_LOW */
    tick(button, false, false); /* DEBOUNCING_HIGH_TO_LOW -> VERIFYING_HIGH_TO_LOW */
    TEST_ASSERT_EQUAL(BUTTON_VERIFYING_HIGH_TO_LOW, *button);
}

/* Settles VERIFYING_LOW_TO_HIGH into a registered press, asserting the edge fires. */
static void settleVerifyingLowToHighAsPress(ButtonDebounceState *button)
{
    ButtonDebounceFSMOutput out = tick(button, true, false);
    TEST_ASSERT_EQUAL(BUTTON_HIGH, *button);
    TEST_ASSERT_EQUAL(BUTTON_EDGE_LOW_TO_HIGH, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/* Settles VERIFYING_HIGH_TO_LOW into a registered release, asserting the edge fires. */
static void settleVerifyingHighToLowAsRelease(ButtonDebounceState *button)
{
    ButtonDebounceFSMOutput out = tick(button, false, false);
    TEST_ASSERT_EQUAL(BUTTON_LOW, *button);
    TEST_ASSERT_EQUAL(BUTTON_EDGE_HIGH_TO_LOW, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * initButtonDebounceFSM sets the button to the initial verifying state.
 */
TEST_CASE("initButtonDebounceFSM_setsVerifyingInitState", TEST_GROUP)
{
    ButtonDebounceState button;
    TEST_ASSERT_EQUAL(ESP_OK, initButtonDebounceFSM(&button));
    TEST_ASSERT_EQUAL(BUTTON_VERIFYING_INIT, button);
}

/**
 * With no edge detected right after init, the FSM settles directly to LOW
 * without ever reporting an edge (startup is not a button press).
 */
TEST_CASE("buttonDebounceFSM_settlesToLowFromInitWithoutEdge", TEST_GROUP)
{
    ButtonDebounceState button;
    TEST_ASSERT_EQUAL(ESP_OK, initButtonDebounceFSM(&button));

    ButtonDebounceFSMOutput out = tick(&button, false, false);
    TEST_ASSERT_EQUAL(BUTTON_LOW, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_LOW, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * With no edge detected right after init, the FSM settles directly to HIGH
 * without ever reporting an edge (startup is not a button press).
 */
TEST_CASE("buttonDebounceFSM_settlesToHighFromInitWithoutEdge", TEST_GROUP)
{
    ButtonDebounceState button;
    TEST_ASSERT_EQUAL(ESP_OK, initButtonDebounceFSM(&button));

    ButtonDebounceFSMOutput out = tick(&button, true, false);
    TEST_ASSERT_EQUAL(BUTTON_HIGH, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_HIGH, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * An edge detected while in the initial verifying state starts an
 * (unknown-direction) debounce period rather than settling immediately.
 */
TEST_CASE("buttonDebounceFSM_edgeDuringInitVerifyStartsDebounce", TEST_GROUP)
{
    ButtonDebounceState button;
    TEST_ASSERT_EQUAL(ESP_OK, initButtonDebounceFSM(&button));

    ButtonDebounceFSMOutput out = tick(&button, true, true);
    TEST_ASSERT_EQUAL(BUTTON_DEBOUNCING_INIT, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_UNKNOWN, out.state);
    TEST_ASSERT_FALSE(out.monitorEdges);
    TEST_ASSERT_EQUAL(CONFIG_BUTTON_DEBOUNCE_US, out.debounceTimeUs);
}

/**
 * BUTTON_DEBOUNCING_INIT always advances to BUTTON_VERIFYING_INIT, regardless
 * of buttonHigh/edgeDetected (both are ignored while debouncing an unknown level).
 */
TEST_CASE("buttonDebounceFSM_debouncingInitIgnoresArgsAndAdvancesToVerifyingInit", TEST_GROUP)
{
    ButtonDebounceState button;
    TEST_ASSERT_EQUAL(ESP_OK, initButtonDebounceFSM(&button));
    tick(&button, true, true); /* -> DEBOUNCING_INIT */

    ButtonDebounceFSMOutput out = tick(&button, false, true); /* contradictory args */
    TEST_ASSERT_EQUAL(BUTTON_VERIFYING_INIT, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_UNKNOWN, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(CONFIG_BUTTON_VERIFY_US, out.debounceTimeUs);
}

/**
 * A full initial debounce/verify sequence that settles high still does not
 * report an edge: it is the first read of the button, not a press.
 */
TEST_CASE("buttonDebounceFSM_initialDebounceSettlingHighDoesNotReportEdge", TEST_GROUP)
{
    ButtonDebounceState button;
    TEST_ASSERT_EQUAL(ESP_OK, initButtonDebounceFSM(&button));
    tick(&button, true, true); /* -> DEBOUNCING_INIT */
    tick(&button, true, false); /* -> VERIFYING_INIT */

    ButtonDebounceFSMOutput out = tick(&button, true, false);
    TEST_ASSERT_EQUAL(BUTTON_HIGH, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_HIGH, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * While LOW, no edge leaves the FSM idle in LOW.
 */
TEST_CASE("buttonDebounceFSM_noEdgeStaysLow", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToLow(&button);

    ButtonDebounceFSMOutput out = tick(&button, false, false);
    TEST_ASSERT_EQUAL(BUTTON_LOW, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_LOW, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * While LOW, buttonHigh is irrelevant unless an edge is detected.
 */
TEST_CASE("buttonDebounceFSM_ignoresButtonHighWhileLowAndNoEdge", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToLow(&button);

    ButtonDebounceFSMOutput out = tick(&button, true, false);
    TEST_ASSERT_EQUAL(BUTTON_LOW, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_LOW, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * An edge while LOW starts debouncing a low-to-high transition.
 */
TEST_CASE("buttonDebounceFSM_edgeWhileLowStartsDebounceToHigh", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToLow(&button);

    ButtonDebounceFSMOutput out = tick(&button, true, true);
    TEST_ASSERT_EQUAL(BUTTON_DEBOUNCING_LOW_TO_HIGH, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_LOW, out.state);
    TEST_ASSERT_FALSE(out.monitorEdges);
    TEST_ASSERT_EQUAL(CONFIG_BUTTON_DEBOUNCE_US, out.debounceTimeUs);
}

/**
 * While HIGH, no edge leaves the FSM idle in HIGH.
 */
TEST_CASE("buttonDebounceFSM_noEdgeStaysHigh", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToHigh(&button);

    ButtonDebounceFSMOutput out = tick(&button, true, false);
    TEST_ASSERT_EQUAL(BUTTON_HIGH, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_HIGH, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * While HIGH, buttonHigh is irrelevant unless an edge is detected.
 */
TEST_CASE("buttonDebounceFSM_ignoresButtonHighWhileHighAndNoEdge", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToHigh(&button);

    ButtonDebounceFSMOutput out = tick(&button, false, false);
    TEST_ASSERT_EQUAL(BUTTON_HIGH, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_HIGH, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * An edge while HIGH starts debouncing a high-to-low transition.
 */
TEST_CASE("buttonDebounceFSM_edgeWhileHighStartsDebounceToLow", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToHigh(&button);

    ButtonDebounceFSMOutput out = tick(&button, false, true);
    TEST_ASSERT_EQUAL(BUTTON_DEBOUNCING_HIGH_TO_LOW, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_HIGH, out.state);
    TEST_ASSERT_FALSE(out.monitorEdges);
    TEST_ASSERT_EQUAL(CONFIG_BUTTON_DEBOUNCE_US, out.debounceTimeUs);
}

/**
 * BUTTON_DEBOUNCING_LOW_TO_HIGH always advances to BUTTON_VERIFYING_LOW_TO_HIGH,
 * ignoring buttonHigh/edgeDetected entirely.
 */
TEST_CASE("buttonDebounceFSM_debouncingLowToHighIgnoresArgsAndAdvances", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToLow(&button);
    tick(&button, true, true); /* -> DEBOUNCING_LOW_TO_HIGH */

    ButtonDebounceFSMOutput out = tick(&button, false, false); /* contradictory args */
    TEST_ASSERT_EQUAL(BUTTON_VERIFYING_LOW_TO_HIGH, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_LOW, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(CONFIG_BUTTON_VERIFY_US, out.debounceTimeUs);
}

/**
 * BUTTON_DEBOUNCING_HIGH_TO_LOW always advances to BUTTON_VERIFYING_HIGH_TO_LOW,
 * ignoring buttonHigh/edgeDetected entirely.
 */
TEST_CASE("buttonDebounceFSM_debouncingHighToLowIgnoresArgsAndAdvances", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToHigh(&button);
    tick(&button, false, true); /* -> DEBOUNCING_HIGH_TO_LOW */

    ButtonDebounceFSMOutput out = tick(&button, true, false); /* contradictory args */
    TEST_ASSERT_EQUAL(BUTTON_VERIFYING_HIGH_TO_LOW, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_HIGH, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(CONFIG_BUTTON_VERIFY_US, out.debounceTimeUs);
}

/**
 * An edge detected while verifying low-to-high restarts debouncing (a bounce)
 * instead of registering a press.
 */
TEST_CASE("buttonDebounceFSM_bounceDuringVerifyLowToHighRestartsDebounce", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToLow(&button);
    enterVerifyingLowToHigh(&button);

    ButtonDebounceFSMOutput out = tick(&button, false, true);
    TEST_ASSERT_EQUAL(BUTTON_DEBOUNCING_LOW_TO_HIGH, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_LOW, out.state);
    TEST_ASSERT_FALSE(out.monitorEdges);
    TEST_ASSERT_EQUAL(CONFIG_BUTTON_DEBOUNCE_US, out.debounceTimeUs);
}

/**
 * Verifying low-to-high with the button back low settles to LOW without
 * ever reporting a press edge.
 */
TEST_CASE("buttonDebounceFSM_failedVerifyLowToHighSettlesLowWithoutEdge", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToLow(&button);
    enterVerifyingLowToHigh(&button);

    ButtonDebounceFSMOutput out = tick(&button, false, false);
    TEST_ASSERT_EQUAL(BUTTON_LOW, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_LOW, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * A full low-to-high debounce/verify sequence reports exactly one
 * BUTTON_EDGE_LOW_TO_HIGH at the moment it settles.
 */
TEST_CASE("buttonDebounceFSM_fullLowToHighSequenceReportsEdgeOnSettle", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToLow(&button);
    enterVerifyingLowToHigh(&button);
    settleVerifyingLowToHighAsPress(&button);
}

/**
 * An edge detected while verifying high-to-low restarts debouncing (a bounce)
 * instead of registering a release.
 */
TEST_CASE("buttonDebounceFSM_bounceDuringVerifyHighToLowRestartsDebounce", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToHigh(&button);
    enterVerifyingHighToLow(&button);

    ButtonDebounceFSMOutput out = tick(&button, true, true);
    TEST_ASSERT_EQUAL(BUTTON_DEBOUNCING_HIGH_TO_LOW, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_HIGH, out.state);
    TEST_ASSERT_FALSE(out.monitorEdges);
    TEST_ASSERT_EQUAL(CONFIG_BUTTON_DEBOUNCE_US, out.debounceTimeUs);
}

/**
 * Verifying high-to-low with the button back high settles to HIGH without
 * ever reporting a release edge.
 */
TEST_CASE("buttonDebounceFSM_failedVerifyHighToLowSettlesHighWithoutEdge", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToHigh(&button);
    enterVerifyingHighToLow(&button);

    ButtonDebounceFSMOutput out = tick(&button, true, false);
    TEST_ASSERT_EQUAL(BUTTON_HIGH, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_HIGH, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(0, out.debounceTimeUs);
}

/**
 * A full high-to-low debounce/verify sequence reports exactly one
 * BUTTON_EDGE_HIGH_TO_LOW at the moment it settles.
 */
TEST_CASE("buttonDebounceFSM_fullHighToLowSequenceReportsEdgeOnSettle", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToHigh(&button);
    enterVerifyingHighToLow(&button);
    settleVerifyingHighToLowAsRelease(&button);
}

/**
 * Multiple consecutive bounces while verifying low-to-high each restart
 * debouncing, and only a stable high finally reports a single press.
 */
TEST_CASE("buttonDebounceFSM_multipleBouncesDuringVerifyLowToHighSettleIntoSinglePress", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToLow(&button);
    enterVerifyingLowToHigh(&button);

    for (int bounce = 0; bounce < 3; bounce++)
    {
        ButtonDebounceFSMOutput out = tick(&button, false, true); /* bounce */
        TEST_ASSERT_EQUAL(BUTTON_DEBOUNCING_LOW_TO_HIGH, button);
        TEST_ASSERT_EQUAL(CONFIG_BUTTON_DEBOUNCE_US, out.debounceTimeUs);

        out = tick(&button, true, false); /* back to verifying */
        TEST_ASSERT_EQUAL(BUTTON_VERIFYING_LOW_TO_HIGH, button);
        TEST_ASSERT_EQUAL(CONFIG_BUTTON_VERIFY_US, out.debounceTimeUs);
    }

    settleVerifyingLowToHighAsPress(&button);
}

/**
 * Multiple consecutive bounces while verifying high-to-low each restart
 * debouncing, and only a stable low finally reports a single release.
 */
TEST_CASE("buttonDebounceFSM_multipleBouncesDuringVerifyHighToLowSettleIntoSingleRelease", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToHigh(&button);
    enterVerifyingHighToLow(&button);

    for (int bounce = 0; bounce < 3; bounce++)
    {
        ButtonDebounceFSMOutput out = tick(&button, true, true); /* bounce */
        TEST_ASSERT_EQUAL(BUTTON_DEBOUNCING_HIGH_TO_LOW, button);
        TEST_ASSERT_EQUAL(CONFIG_BUTTON_DEBOUNCE_US, out.debounceTimeUs);

        out = tick(&button, false, false); /* back to verifying */
        TEST_ASSERT_EQUAL(BUTTON_VERIFYING_HIGH_TO_LOW, button);
        TEST_ASSERT_EQUAL(CONFIG_BUTTON_VERIFY_US, out.debounceTimeUs);
    }

    settleVerifyingHighToLowAsRelease(&button);
}

/**
 * Repeated press/release cycles starting from LOW each report exactly one
 * press edge and one release edge, never more.
 */
TEST_CASE("buttonDebounceFSM_repeatedCyclesFromLowEachReportExactlyOneEdge", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToLow(&button);

    for (int cycle = 0; cycle < 3; cycle++)
    {
        enterVerifyingLowToHigh(&button);
        settleVerifyingLowToHighAsPress(&button);

        enterVerifyingHighToLow(&button);
        settleVerifyingHighToLowAsRelease(&button);
    }
}

/**
 * Repeated press/release cycles starting from HIGH each report exactly one
 * release edge and one press edge, never more.
 */
TEST_CASE("buttonDebounceFSM_repeatedCyclesFromHighEachReportExactlyOneEdge", TEST_GROUP)
{
    ButtonDebounceState button;
    settleInitToHigh(&button);

    for (int cycle = 0; cycle < 3; cycle++)
    {
        enterVerifyingHighToLow(&button);
        settleVerifyingHighToLowAsRelease(&button);

        enterVerifyingLowToHigh(&button);
        settleVerifyingLowToHighAsPress(&button);
    }
}

/**
 * An unexpected/corrupted state value heals back to the initial verifying
 * state rather than leaving the FSM stuck or reading out of bounds.
 */
TEST_CASE("buttonDebounceFSM_healsFromInvalidState", TEST_GROUP)
{
    ButtonDebounceState button = (ButtonDebounceState) 0x7FFFFFFF;

    ButtonDebounceFSMOutput out = tick(&button, false, false);
    TEST_ASSERT_EQUAL(BUTTON_VERIFYING_INIT, button);
    TEST_ASSERT_EQUAL(BUTTON_LEVEL_UNKNOWN, out.state);
    TEST_ASSERT_TRUE(out.monitorEdges);
    TEST_ASSERT_EQUAL(CONFIG_BUTTON_VERIFY_US, out.debounceTimeUs);
}
