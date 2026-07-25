/**
 * test_refresh_fsm.c
 *
 * Unit tests for refresh_fsm.h.
 *
 * Expected FSM states:
 * - WAITING_FOR_FRAMES (displaying loading animation (untested), initial state, waiting a frame
 * and typical frames before transitioning to INSTALLING_FRAME)
 * - INSTALLING_FRAME (setting LEDs on in frame sequence, ending at FRAME_INSTALLED)
 * - CLEARING_FRAME (setting LEDs off in opposite frame sequence from last set LED,
 * ending at CLEARED if night mode on, otherwise ends at INSTALLING_FRAME)
 * - QUICK_CLEARING_FRAME (setting action to quick clear frame in opposite frame sequence
 * from the last set LED and immediately transitioning to CLEARED if night mode on,
 * otherwise immediately transitions to INSTALLING_FRAME)
 * - CLEARED (no action, all LEDs are off)
 * - FRAME_INSTALLED (no action, LEDs match frame)
 *
 * Expected command behaviors by state:
 * - FSM_WAITING_FOR_FRAMES:
 *      NEW_FRAME: if north/south typical frames exist, enter INSTALLING_FRAME if night mode
 *                 is disabled. Otherwise, enter CLEARED.
 *      UPDATE_TYPICAL: if north/south typical frames and a frame exists, enter INSTALLING_FRAME
 *                      if night mode is disabled. Otherwise, enter CLEARED.
 *      NIGHT_MODE_ON: set night mode on.
 *      NIGHT_MODE_OFF: set night mode off.
 * - FSM_INSTALLING_FRAME:
 *      NEW_FRAME: buffer new frame, which will be the current frame once INSTALLING_FRAME is
 *                 entered again. Transition to CLEARING_FRAME.
 *      UPDATE_TYPICAL: buffer new typical frame, which will be latched once INSTALLING_FRAME
 *                      is entered again.
 *      NIGHT_MODE_ON: set night mode on. Transition to CLEARING_FRAME.
 *      NIGHT_MODE_OFF: set night mode off. (not be possible for night mode to be on here)
 * - FSM_CLEARING_FRAME:
 *      NEW_FRAME: Transition to QUICK_CLEARING_FRAME.
 *      UPDATE_TYPICAL: buffer new typical frame, which will be latched once INSTALLING_FRAME
 *                      is entered again.
 *      NIGHT_MODE_ON: set night mode on.
 *      NIGHT_MODE_OFF: set night mode off.
 * - FSM_QUICK_CLEARING_FRAME: this state is instantaneous. It is not possible to handle
 *      commands in this state.
 * - FSM_CLEARED:
 *      NEW_FRAME: latch new frame and enter INSTALLING_FRAME.
 *      UPDATE_TYPICAL: buffer new typical frame, which will be latched once INSTALLING_FRAME
 *                      is entered again.
 *      NIGHT_MODE_ON: set night mode on.
 *      NIGHT_MODE_OFF: set night mode off. Enter INSTALLING_FRAME.
 * - FSM_FRAME_INSTALLED:
 *      NEW_FRAME: buffer new frame, which will be the current frame once INSTALLING_FRAME is
 *                 entered again. Transition to CLEARING_FRAME.
 *      UPDATE_TYPICAL: buffer new typical frame, which will be latched once INSTALLING_FRAME
 *                      is entered again.
 *      NIGHT_MODE_ON: set night mode on. Enter CLEARING_FRAME.
 *      NIGHT_MODE_OFF: set night mode off.
 *
 * Expected IO invariants:
 * - frame lifecycle correctness (the frames are released at the correct time)
 * - idle consistency (the idle flag is thrown when the FSM is in the WAITING_FOR_FRAMES,
 * CLEARED, and FRAME_INSTALLED states)
 *
 * Test file dependencies: None.
 */

#include "refresh_fsm.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "unity_test_runner.h"
#include "led_types.h"

#define TEST_GROUP      "refresh_fsm"

#define TEST_FRAME_SIZE         (5)
#define TEST_MAX_FRAME_SLOTS    (8)

#define TEST_SLOW_LED_RED       (0xFA)
#define TEST_SLOW_LED_GREEN     (0x78)
#define TEST_SLOW_LED_BLUE      (0x65)

#define TEST_MED_LED_RED        (0x16)
#define TEST_MED_LED_GREEN      (0xA8)
#define TEST_MED_LED_BLUE       (0x2B)

#define TEST_FAST_LED_RED       (0x0F)
#define TEST_FAST_LED_GREEN     (0x88)
#define TEST_FAST_LED_BLUE      (0x73)

/**
 * An LED context in which to run tests.
 *
 * The frame index fields (mockTypicalNorthFrameNdx, mockTypicalSouthFrameNdx,
 * mockCurrentFrameNdx) represent the indices the FSM will track internally.
 * Tests use these values to verify that the FSM returns the correct indices
 * in its release outputs. The actual LED speed data lives in the three speed
 * arrays and is referenced at those indices inside initTestFSM.
 *
 * Constraints on test data:
 * - All ledNum values in currentSpeeds must be in [0, TEST_FRAME_SIZE).
 * - For any entry in currentSpeeds with ledNum N, typicalNorthSpeeds[N].ledNum
 *   and typicalSouthSpeeds[N].ledNum must equal N (typical frames are indexed
 *   by LED number inside the FSM).
 * - The three mock frame index values must be distinct and in [0, TEST_MAX_FRAME_SLOTS).
 */
typedef struct {
    LEDSpeed typicalNorthSpeeds[TEST_FRAME_SIZE];
    LEDSpeed typicalSouthSpeeds[TEST_FRAME_SIZE];
    LEDSpeed currentSpeeds[TEST_FRAME_SIZE];
    uint32_t mockTypicalNorthFrameNdx;
    uint32_t mockTypicalSouthFrameNdx;
    uint32_t mockCurrentFrameNdx;
    Direction currDir;
} TestFrameContext;

/**
 * Loads the context into the stated indices of frames.
 * 
 * @param frames The frame array sent to the FSM.
 * @param ctx The context to load into frames.
 */
static void loadContext(TestFrameContext *ctx, LEDSpeed **frames)
{
    frames[ctx->mockTypicalNorthFrameNdx] = ctx->typicalNorthSpeeds;
    frames[ctx->mockTypicalSouthFrameNdx] = ctx->typicalSouthSpeeds;
    frames[ctx->mockCurrentFrameNdx] = ctx->currentSpeeds;
}

/**
 * Initializes the refresh FSM with test resources derived from ctx.
 * All LED numbers in [0, TEST_FRAME_SIZE) are marked valid.
 *
 * @param ctx The context describing the frame indices and LED speed data
 * the FSM should use.
 * @param frames The caller-owned LEDSpeed* array of length TEST_MAX_FRAME_SLOTS
 * to use as the FSM's frame store. The caller must point each required index
 * to the appropriate LEDSpeed data before calling this function.
 */
static void initTestFSM(TestFrameContext *ctx, LEDSpeed **frames)
{
    static LEDReg ledNumToReg[TEST_FRAME_SIZE];

    for (int i = 0; i < TEST_FRAME_SIZE; i++)
    {
        ledNumToReg[i] = (LEDReg){ .matrix = MAT1_PAGE0, .red = 0, .green = 0, .blue = 0 };
    }

    RefreshFSMResources resources = {
        .LEDNumToReg    = ledNumToReg,
        .LEDNumToRegLen = TEST_FRAME_SIZE,
        .LEDFrames      = frames,
        .LEDFramesLen   = TEST_MAX_FRAME_SLOTS,
        .slowLEDColor   = { .red = TEST_SLOW_LED_RED, .green = TEST_SLOW_LED_GREEN, .blue = TEST_SLOW_LED_BLUE },
        .mediumLEDColor = { .red = TEST_MED_LED_RED, .green = TEST_MED_LED_GREEN, .blue = TEST_MED_LED_BLUE },
        .fastLEDColor   = { .red = TEST_FAST_LED_RED, .green = TEST_FAST_LED_GREEN, .blue = TEST_FAST_LED_BLUE },
    };

    refreshFSMInit(&resources);
}

/**
 * Initializes the FSM into the INSTALLING_FRAME state with the given context.
 * 
 * @note The first LED set, at index 0, is checked by this function.
 * The caller should expect that the next LED to be set is at index 1.
 * 
 * @param ctx The context describing the frame indices and LED speed data.
 * @param frames The caller-owned frame store passed to the FSM at init.
 */
static void testSetupInstallingFrameState(TestFrameContext *ctx, LEDSpeed **frames)
{
    RefreshFSMCommand cmd;
    RefreshFSMOutput out;

    initTestFSM(ctx, frames);
    
    /* provide north typical frame */
    frames[ctx->mockTypicalNorthFrameNdx] = ctx->typicalNorthSpeeds;
    cmd = (RefreshFSMCommand){
        .type     = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = ctx->mockTypicalNorthFrameNdx,
        .frameLen = TEST_FRAME_SIZE,
        .dir      = NORTH,
    };
    out = refreshFSMTick(&cmd);
    TEST_ASSERT_TRUE(out.isIdle);
    TEST_ASSERT_EQUAL_HEX(REFRESH_ACTION_NONE, out.action);

    /* provide south typical frame */
    frames[ctx->mockTypicalSouthFrameNdx] = ctx->typicalSouthSpeeds;
    cmd = (RefreshFSMCommand){
        .type     = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = ctx->mockTypicalSouthFrameNdx,
        .frameLen = TEST_FRAME_SIZE,
        .dir      = SOUTH,
    };
    refreshFSMTick(&cmd);
    TEST_ASSERT_TRUE(out.isIdle);
    TEST_ASSERT_EQUAL_HEX(REFRESH_ACTION_NONE, out.action);

    /* provide current frame — triggers transition to INSTALLING_FRAME and
     * processes the first LED in the same tick */
    cmd = (RefreshFSMCommand){
        .type     = REFRESH_CMD_NEW_FRAME,
        .frameNdx = ctx->mockCurrentFrameNdx,
        .frameLen = TEST_FRAME_SIZE,
        .dir      = ctx->currDir,
    };
    out = refreshFSMTick(&cmd);
    TEST_ASSERT_FALSE(out.isIdle);
    TEST_ASSERT_EQUAL_HEX(REFRESH_ACTION_SET, out.action);
    TEST_ASSERT_EQUAL_UINT16(ctx->currentSpeeds[0].ledNum, out.actionParams.set.ledNum);

    /* verify INSTALLING_FRAME was entered: first LED must be set */
    TEST_ASSERT_EQUAL(REFRESH_ACTION_SET, out.action);
    TEST_ASSERT_EQUAL(ctx->currentSpeeds[0].ledNum, out.actionParams.set.ledNum);
}

/**
 * Initializes the FSM into the CLEARING_FRAME state with the given context.
 * 
 * @note The first LED cleared is the 2nd set, at index 1. This is
 * checked by this setup function. The caller should expect that one
 * more LED will be cleared, which was the 1st set, at index 0.
 * 
 * @param ctx The context describing the frame indices and LED speed data.
 * @param frames The caller-owned frame store passed to the FSM at init.
 */
static void testSetupClearingFrameState(TestFrameContext *ctx, LEDSpeed **frames)
{
    RefreshFSMCommand cmd;
    RefreshFSMOutput out;

    initTestFSM(ctx, frames);
    testSetupInstallingFrameState(ctx, frames);

    /* send no command to move to next LED */
    cmd = (RefreshFSMCommand) {
        .type = REFRESH_CMD_NONE,
    };
    out = refreshFSMTick(&cmd);
    TEST_ASSERT_FALSE(out.isIdle);
    TEST_ASSERT_EQUAL(REFRESH_ACTION_SET, out.action);
    TEST_ASSERT_EQUAL(ctx->currentSpeeds[1].ledNum, out.actionParams.set.ledNum);

    /* send NEW_FRAME command to enter CLEARING_FRAME state */
    cmd = (RefreshFSMCommand) {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = ctx->mockCurrentFrameNdx,
        .frameLen = TEST_FRAME_SIZE,
        .dir = ctx->currDir,
    };
    out = refreshFSMTick(&cmd);
    TEST_ASSERT_FALSE(out.isIdle);
    TEST_ASSERT_EQUAL(REFRESH_ACTION_CLEAR, out.action);
    TEST_ASSERT_EQUAL(ctx->currentSpeeds[1].ledNum, out.actionParams.clear.ledNum);
}

/**
 * Initializes the FSM through the QUICK_CLEARING_FRAME state with the given context.
 * 
 * @note The FSM will end up at the INSTALLING_FRAME state with the 0 index LED set
 * at the next tick if night mode is disabled. Otherwise, the FSM will end up at the
 * CLEARED state, which is checked in this function.
 *
 * Drives the FSM to CLEARING_FRAME, then sends another NEW_FRAME command
 * which transitions to QUICK_CLEARING_FRAME. Because QUICK_CLEARING_FRAME
 * is instantaneous, the state handler runs in the same tick and transitions
 * the FSM to INSTALLING_FRAME (night mode is off). The emitted CLEAR_RANGE
 * action confirms that QUICK_CLEARING_FRAME was entered.
 *
 * @param ctx The context describing the frame indices and LED speed data.
 * @param frames The caller-owned frame store passed to the FSM at init.
 * @param nightModeEn Whether to enable night mode just before entering the
 * QUICK_CLEARING_FRAME state or not.
 */
static void testSetupQuickClearingFrameState(TestFrameContext *ctx, LEDSpeed **frames, bool nightModeEn)
{
    RefreshFSMCommand cmd;
    RefreshFSMOutput out;

    initTestFSM(ctx, frames);
    testSetupInstallingFrameState(ctx, frames);

    /* send no command to move to LED 2 */
    cmd = (RefreshFSMCommand) {
        .type = REFRESH_CMD_NONE,
    };
    out = refreshFSMTick(&cmd);
    TEST_ASSERT_FALSE(out.isIdle);
    TEST_ASSERT_EQUAL(REFRESH_ACTION_SET, out.action);
    TEST_ASSERT_EQUAL(ctx->currentSpeeds[1].ledNum, out.actionParams.set.ledNum);

    /* send no command to move to LED 3 */
    cmd = (RefreshFSMCommand) {
        .type = REFRESH_CMD_NONE,
    };
    out = refreshFSMTick(&cmd);
    TEST_ASSERT_FALSE(out.isIdle);
    TEST_ASSERT_EQUAL(REFRESH_ACTION_SET, out.action);
    TEST_ASSERT_EQUAL(ctx->currentSpeeds[2].ledNum, out.actionParams.set.ledNum);

    /* send NEW_FRAME command to enter CLEARING_FRAME state */
    cmd = (RefreshFSMCommand) {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = ctx->mockCurrentFrameNdx,
        .frameLen = TEST_FRAME_SIZE,
        .dir = ctx->currDir,
    };
    out = refreshFSMTick(&cmd);
    TEST_ASSERT_FALSE(out.isIdle);
    TEST_ASSERT_EQUAL(REFRESH_ACTION_CLEAR, out.action);
    TEST_ASSERT_EQUAL(ctx->currentSpeeds[2].ledNum, out.actionParams.clear.ledNum);

    if (nightModeEn)
    {
        /* send NIGHT_MODE_ON command */
        cmd = (RefreshFSMCommand) {
            .type = REFRESH_CMD_NIGHT_MODE_ON,
        };
        out = refreshFSMTick(&cmd);
        TEST_ASSERT_EQUAL(REFRESH_ACTION_CLEAR, out.action);
        TEST_ASSERT_EQUAL(ctx->currentSpeeds[1].ledNum, out.actionParams.clear.ledNum);
    }

    /* send NEW_FRAME command to enter QUICK_CLEARING state */
    cmd = (RefreshFSMCommand) {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = ctx->mockCurrentFrameNdx,
        .frameLen = TEST_FRAME_SIZE,
        .dir = ctx->currDir,
    };
    out = refreshFSMTick(&cmd);
    TEST_ASSERT_EQUAL(REFRESH_ACTION_CLEAR_RANGE, out.action);
    if (nightModeEn)
    {
        TEST_ASSERT_EQUAL(ctx->currentSpeeds[0].ledNum, out.actionParams.clearRange.startLedNum);
        TEST_ASSERT_TRUE(out.isIdle);
    } else
    {
        TEST_ASSERT_EQUAL(ctx->currentSpeeds[1].ledNum, out.actionParams.clearRange.startLedNum);
        TEST_ASSERT_FALSE(out.isIdle);
    }
}

/**
 * Initializes the FSM into the CLEARED state.
 * 
 * @note The CLEARED state should only ever be entered if night mode is
 * enabled, therefore night mode will be enabled after this function.
 *
 * @param ctx The context describing the frame indices and LED speed data.
 * @param frames The caller-owned frame store passed to the FSM at init.
 */
static void testSetupClearedState(TestFrameContext *ctx, LEDSpeed **frames)
{   
    initTestFSM(ctx, frames);
    const bool nightModeEn = true;
    testSetupQuickClearingFrameState(ctx, frames, nightModeEn);
}

/**
 * Ticks the FSM into the FRAME_INSTALLED state.
 *
 * Drives the FSM to INSTALLING_FRAME, then ticks through all TEST_FRAME_SIZE
 * LEDs. When the last LED is processed the FSM transitions to FRAME_INSTALLED.
 *
 * @param ctx The context describing the frame indices and LED speed data.
 * @param frames The caller-owned frame store passed to the FSM at init.
 */
static void testSetupFrameInstalledState(TestFrameContext *ctx, LEDSpeed **frames)
{
    RefreshFSMCommand cmd;
    RefreshFSMOutput out;
    
    initTestFSM(ctx, frames);
    testSetupInstallingFrameState(ctx, frames);

    cmd = (RefreshFSMCommand) {
        .type = REFRESH_CMD_NONE,
    };
    out = refreshFSMTick(&cmd);
    TEST_ASSERT_FALSE(out.isIdle);
    TEST_ASSERT_EQUAL(REFRESH_ACTION_SET, out.action);

    for (uint32_t i = 0; i < 100; i++)
    {
        if (REFRESH_ACTION_NONE == out.action)
        {
            TEST_ASSERT_TRUE(out.isIdle);
            break;
        } else
        {
            TEST_ASSERT_FALSE(out.isIdle);
            TEST_ASSERT_EQUAL(REFRESH_ACTION_SET, out.action);
        }

        out = refreshFSMTick(&cmd);
    }
}

TEST_CASE("refreshFSM_canEnterInstallingFrameState", TEST_GROUP)
{
    LEDSpeed *frames[TEST_MAX_FRAME_SLOTS];
    TestFrameContext ctx = {
        .typicalNorthSpeeds = { {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5} },
        .typicalSouthSpeeds = { {0, 6}, {1, 7}, {2, 8}, {3, 9}, {4, 10} },
        .currentSpeeds      = { {0, 11}, {1, 12}, {2, 13}, {3, 14}, {4, 15} },
        .mockTypicalNorthFrameNdx = 0,
        .mockTypicalSouthFrameNdx = 1,
        .mockCurrentFrameNdx = 2,
        .currDir = NORTH,
    };
    loadContext(&ctx, frames);

    testSetupInstallingFrameState(&ctx, frames);
}

TEST_CASE("refreshFSM_canEnterClearingState", TEST_GROUP)
{
    LEDSpeed *frames[TEST_MAX_FRAME_SLOTS];
    TestFrameContext ctx = {
        .typicalNorthSpeeds = { {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5} },
        .typicalSouthSpeeds = { {0, 6}, {1, 7}, {2, 8}, {3, 9}, {4, 10} },
        .currentSpeeds      = { {0, 11}, {1, 12}, {2, 13}, {3, 14}, {4, 15} },
        .mockTypicalNorthFrameNdx = 0,
        .mockTypicalSouthFrameNdx = 1,
        .mockCurrentFrameNdx = 2,
        .currDir = NORTH,
    };
    loadContext(&ctx, frames);

    testSetupClearingFrameState(&ctx, frames);
}

TEST_CASE("refreshFSM_canEnterQuickClearingState", TEST_GROUP)
{
    LEDSpeed *frames[TEST_MAX_FRAME_SLOTS];
    TestFrameContext ctx = {
        .typicalNorthSpeeds = { {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5} },
        .typicalSouthSpeeds = { {0, 6}, {1, 7}, {2, 8}, {3, 9}, {4, 10} },
        .currentSpeeds      = { {0, 11}, {1, 12}, {2, 13}, {3, 14}, {4, 15} },
        .mockTypicalNorthFrameNdx = 0,
        .mockTypicalSouthFrameNdx = 1,
        .mockCurrentFrameNdx = 2,
        .currDir = NORTH,
    };
    loadContext(&ctx, frames);

    const bool nightMode = false;
    testSetupQuickClearingFrameState(&ctx, frames, nightMode);
}

TEST_CASE("refreshFSM_canEnterQuickClearingStateInNightMode", TEST_GROUP)
{
    LEDSpeed *frames[TEST_MAX_FRAME_SLOTS];
    TestFrameContext ctx = {
        .typicalNorthSpeeds = { {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5} },
        .typicalSouthSpeeds = { {0, 6}, {1, 7}, {2, 8}, {3, 9}, {4, 10} },
        .currentSpeeds      = { {0, 11}, {1, 12}, {2, 13}, {3, 14}, {4, 15} },
        .mockTypicalNorthFrameNdx = 0,
        .mockTypicalSouthFrameNdx = 1,
        .mockCurrentFrameNdx = 2,
        .currDir = NORTH,
    };
    loadContext(&ctx, frames);

    const bool nightMode = false;
    testSetupQuickClearingFrameState(&ctx, frames, nightMode);
}

TEST_CASE("refreshFSM_canEnterClearedState", TEST_GROUP)
{
    LEDSpeed *frames[TEST_MAX_FRAME_SLOTS];
    TestFrameContext ctx = {
        .typicalNorthSpeeds = { {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5} },
        .typicalSouthSpeeds = { {0, 6}, {1, 7}, {2, 8}, {3, 9}, {4, 10} },
        .currentSpeeds      = { {0, 11}, {1, 12}, {2, 13}, {3, 14}, {4, 15} },
        .mockTypicalNorthFrameNdx = 0,
        .mockTypicalSouthFrameNdx = 1,
        .mockCurrentFrameNdx = 2,
        .currDir = NORTH,
    };
    loadContext(&ctx, frames);

    testSetupClearedState(&ctx, frames);
}

TEST_CASE("refreshFSM_canEnterFrameInstalledState", TEST_GROUP)
{
    LEDSpeed *frames[TEST_MAX_FRAME_SLOTS];
    TestFrameContext ctx = {
        .typicalNorthSpeeds = { {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5} },
        .typicalSouthSpeeds = { {0, 6}, {1, 7}, {2, 8}, {3, 9}, {4, 10} },
        .currentSpeeds      = { {0, 11}, {1, 12}, {2, 13}, {3, 14}, {4, 15} },
        .mockTypicalNorthFrameNdx = 0,
        .mockTypicalSouthFrameNdx = 1,
        .mockCurrentFrameNdx = 2,
        .currDir = NORTH,
    };
    loadContext(&ctx, frames);

    testSetupFrameInstalledState(&ctx, frames);
}


// typedef struct {
//     LEDSpeed typicalNorthSpeeds[TEST_FRAME_SIZE];
//     LEDSpeed typicalSouthSpeeds[TEST_FRAME_SIZE];
//     LEDSpeed currentSpeeds[TEST_FRAME_SIZE];
//     uint32_t mockTypicalNorthFrameNdx;
//     uint32_t mockTypicalSouthFrameNdx;
//     uint32_t mockCurrentFrameNdx;
//     Direction currDir;
// } TestFrameContext;