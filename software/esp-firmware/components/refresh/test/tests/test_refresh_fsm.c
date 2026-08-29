/**
 * test_refresh_fsm.c
 *
 * Unit tests for refresh_fsm.h.
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

#define NUM_TEST_FRAMES         (7)

static LEDReg ledNumToReg[MAX_FRAME_SIZE];
static LEDSpeed ledFrames[NUM_TEST_FRAMES][MAX_FRAME_SIZE];

TEST_CASE("defaultInitialization", TEST_GROUP)
{
    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NONE */
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NONE,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_TRUE(out1.isIdle);
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
}

TEST_CASE("initialFrameInstallation", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i + 3;
    }
    
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE - 3,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(3, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 4; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i + 3, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);
}

TEST_CASE("frameRefresh", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* send REFRESH_CMD_REFRESH */
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_REFRESH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are cleared */
    for (int32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMCommand cmd7 = { .type = REFRESH_CMD_NONE, };
        RefreshFSMOutput out7 = refreshFSMTick(&cmd7);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMCommand cmd7 = { .type = REFRESH_CMD_NONE, };
    RefreshFSMOutput out7 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out7.action.clear.ledNum);
    TEST_ASSERT_FALSE(out7.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set again */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMCommand cmd8 = { .type = REFRESH_CMD_NONE, };
        RefreshFSMOutput out8 = refreshFSMTick(&cmd8);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out8.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out8.action.set.ledNum);
        TEST_ASSERT_FALSE(out8.isIdle);
    }

    RefreshFSMCommand cmd9 = { .type = REFRESH_CMD_NONE, };
    RefreshFSMOutput out9 = refreshFSMTick(&cmd9);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out9.action.clear.ledNum);
    TEST_ASSERT_TRUE(out9.isIdle);
}
 
TEST_CASE("newFrameInstallation", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* send REFRESH_CMD_NEW_FRAME w/ different direction from what is installed */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = SOUTH,
    };

    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are cleared */
    RefreshFSMCommand cmd7 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmd7);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(1, out8.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out8.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out8.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out8.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the new frame are installed */
    RefreshFSMOutput out9 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out9.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_FALSE(out9.isIdle);

    for (uint32_t i = 21; i < 20 + 14; i++)
    {
        RefreshFSMOutput out10 = refreshFSMTick(&cmd7);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out10.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
        TEST_ASSERT_FALSE(out10.isIdle);
    }

    RefreshFSMOutput out11 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20 + 14, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_TRUE(out11.isIdle);
}

TEST_CASE("typicalFrameUpdate_ReleasesQueuedFrame", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;
    const uint32_t cmd7FrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL to queue a typical frame */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd6FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_TRUE(out6.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd7FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd7 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd7FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out7 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out7.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, out7.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd6FrameNdx, out7.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL(REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL, out7.framesToRelease.list[0].type);
    TEST_ASSERT_TRUE(out7.isIdle);
}

TEST_CASE("typicalFrameUpdate_ReleasesOldFrame", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;
    const uint32_t cmd7FrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL to queue a typical frame */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd6FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_TRUE(out6.isIdle);

    /* send REFRESH_CMD_NEW_FRAME to force use of new typical frame */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd7FrameNdx][i].ledNum = 15 + i;
    }

    RefreshFSMCommand cmd7 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd7FrameNdx,
        .frameLen = 15,
        .dir = SOUTH,
    };
    RefreshFSMOutput out7 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out7.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
    TEST_ASSERT_FALSE(out7.isIdle);

    /* send REFRESH_CMD_NONE until old frame is cleared */
    RefreshFSMCommand cmd8 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out8 = refreshFSMTick(&cmd8);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out8.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
        TEST_ASSERT_FALSE(out8.isIdle);
    }

    RefreshFSMOutput out9 = refreshFSMTick(&cmd8);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out9.action.clear.ledNum);
    TEST_ASSERT_EQUAL(2, out9.framesToRelease.len);
    if (cmd1FrameNdx == out9.framesToRelease.list[0].index)
    {
        TEST_ASSERT_EQUAL(cmd1FrameNdx, out9.framesToRelease.list[0].index);
        TEST_ASSERT_EQUAL(REFRESH_FSM_FRAME_RELEASE_STANDARD, out9.framesToRelease.list[0].type);
        TEST_ASSERT_EQUAL(cmd3FrameNdx, out9.framesToRelease.list[1].index);
        TEST_ASSERT_EQUAL(REFRESH_FSM_FRAME_RELEASE_TYPICAL_SOUTH, out9.framesToRelease.list[1].type);
    } else
    {
        TEST_ASSERT_EQUAL(cmd1FrameNdx, out9.framesToRelease.list[1].index);
        TEST_ASSERT_EQUAL(REFRESH_FSM_FRAME_RELEASE_STANDARD, out9.framesToRelease.list[1].type);
        TEST_ASSERT_EQUAL(cmd3FrameNdx, out9.framesToRelease.list[0].index);
        TEST_ASSERT_EQUAL(REFRESH_FSM_FRAME_RELEASE_TYPICAL_SOUTH, out9.framesToRelease.list[0].type);
    }

    TEST_ASSERT_FALSE(out9.isIdle);

    /* send REFRESH_CMD_NONE until new frame is installed */
    for (uint32_t i = 15; i < 15 + 14; i++)
    {
        RefreshFSMOutput out10 = refreshFSMTick(&cmd8);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out10.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
        TEST_ASSERT_FALSE(out10.isIdle);
    }

    RefreshFSMOutput out11 = refreshFSMTick(&cmd8);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(29, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_TRUE(out11.isIdle);
}

TEST_CASE("typicalFrameUpdateDuringClearing", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;
    const uint32_t cmd7FrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (1) frame is fully installed and FSM is idle. Now send
       REFRESH_CMD_NEW_FRAME with the same direction (NORTH) as installed.
       The FSM queues the frame and begins clearing the board in reverse
       order. */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    /* send REFRESH_CMD_NONE to continue clearing partway through the board */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 10; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    /* (3) while REFRESH_ACTION_CLEAR is still being output, send
       REFRESH_CMD_UPDATE_TYPICAL with the same direction (NORTH) as the
       frame of (2). The FSM queues the new typical frame and clearing
       continues uninterrupted in the same tick. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd7FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd7 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd7FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out8 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(10, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_FALSE(out8.isIdle);

    /* continue clearing down to LED 1 */
    for (uint32_t i = 9; i > 0; i--)
    {
        RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out9.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out9.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
        TEST_ASSERT_FALSE(out9.isIdle);
    }

    /* (3) the final REFRESH_ACTION_CLEAR, for LED 0 (the first LED of frame
       (1)), releases the old standard frame of (1) and the old NORTH
       typical frame of (1) in the same output. */
    RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out10.action.clear.ledNum);
    TEST_ASSERT_EQUAL(2, out10.framesToRelease.len);
    if (cmd1FrameNdx == out10.framesToRelease.list[0].index)
    {
        TEST_ASSERT_EQUAL(cmd1FrameNdx, out10.framesToRelease.list[0].index);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out10.framesToRelease.list[0].type, "refresh frame release type");
        TEST_ASSERT_EQUAL(cmd2FrameNdx, out10.framesToRelease.list[1].index);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_TYPICAL_NORTH, out10.framesToRelease.list[1].type, "refresh frame release type");
    } else
    {
        TEST_ASSERT_EQUAL(cmd2FrameNdx, out10.framesToRelease.list[0].index);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_TYPICAL_NORTH, out10.framesToRelease.list[0].type, "refresh frame release type");
        TEST_ASSERT_EQUAL(cmd1FrameNdx, out10.framesToRelease.list[1].index);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out10.framesToRelease.list[1].type, "refresh frame release type");
    }
    TEST_ASSERT_FALSE(out10.isIdle);

    /* (4) the FSM outputs REFRESH_ACTION_SET actions in the same order of
       the frame of (2) until all LEDs of the frame are set. The FSM is now
       idle. */
    RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_FALSE(out11.isIdle);

    for (uint32_t i = 21; i < 20 + 14; i++)
    {
        RefreshFSMOutput out12 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out12.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out12.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out12.framesToRelease.len);
        TEST_ASSERT_FALSE(out12.isIdle);
    }

    RefreshFSMOutput out13 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out13.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20 + 14, out13.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out13.framesToRelease.len);
    TEST_ASSERT_TRUE(out13.isIdle);
}

TEST_CASE("RefreshFSMCommandDuringClearing", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (1) frame is fully installed and FSM is idle. Now send
       REFRESH_CMD_NEW_FRAME with the same direction (NORTH) as installed.
       The FSM queues the frame and begins clearing the board in reverse
       order. Note the standard frame release does not happen until the
       clear completes, not on this first tick. */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    /* send REFRESH_CMD_NONE to continue clearing partway through the board */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 10; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    /* (3) while REFRESH_ACTION_CLEAR is still being output, send
       REFRESH_CMD_REFRESH. A refresh (clear then reinstall) is already in
       progress, so the FSM ignores the command: no state change, no
       release, and clearing continues uninterrupted in the same tick. */
    RefreshFSMCommand cmd7 = { .type = REFRESH_CMD_REFRESH };
    RefreshFSMOutput out8 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(10, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_FALSE(out8.isIdle);

    /* continue clearing down to LED 1 */
    for (uint32_t i = 9; i > 0; i--)
    {
        RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out9.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out9.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
        TEST_ASSERT_FALSE(out9.isIdle);
    }

    /* the final REFRESH_ACTION_CLEAR, for LED 0 (the first LED of frame
       (1)), releases the old standard frame of (1). No typical frame was
       ever queued in this test, so it is the only release. */
    RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out10.action.clear.ledNum);
    TEST_ASSERT_EQUAL(1, out10.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out10.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out10.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out10.isIdle);

    /* (4) the FSM outputs REFRESH_ACTION_SET actions in the same order of
       the frame of (2) until all LEDs of the frame are set. The FSM is now
       idle. */
    RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_FALSE(out11.isIdle);

    for (uint32_t i = 21; i < 20 + 14; i++)
    {
        RefreshFSMOutput out12 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out12.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out12.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out12.framesToRelease.len);
        TEST_ASSERT_FALSE(out12.isIdle);
    }

    RefreshFSMOutput out13 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out13.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20 + 14, out13.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out13.framesToRelease.len);
    TEST_ASSERT_TRUE(out13.isIdle);
}

TEST_CASE("typicalFrameUpdate", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) send REFRESH_CMD_UPDATE_TYPICAL with the same direction (NORTH)
       as installed. No typical update was previously queued for this
       direction, so the FSM only buffers the new typical frame: no action
       is output and no frame is released, since the previous typical
       frame is still in use for the currently displayed colors. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd6FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_TRUE(out6.isIdle);

    /* the FSM remains idle with no further output on subsequent
       REFRESH_CMD_NONE ticks */
    RefreshFSMOutput out7 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out7.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
    TEST_ASSERT_TRUE(out7.isIdle);
}

TEST_CASE("standardFrameQueueingDuringClearing", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;
    const uint32_t cmd7FrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) NEW_FRAME frame A is queued (same direction, NORTH). The FSM
       begins clearing the board one LED per tick, in reverse order. */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    /* clear partway through the board, stopping well before frame (1) has
       finished clearing */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i >= 50; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    /* (3) while REFRESH_ACTION_CLEAR is still being output, send another
       REFRESH_CMD_NEW_FRAME (frame B), again with the same direction.
       Because frame A is already queued as the next frame, the FSM
       immediately outputs a FRAME_RELEASE_QUEUED_STANDARD for frame A and
       a FRAME_RELEASE_STANDARD for frame (1) in the same tick, abandons
       one-LED-at-a-time clearing, and instead outputs a single
       REFRESH_ACTION_CLEAR_RANGE (starting from wherever clearing had
       reached) that clears the remainder of the board and immediately
       begins installing frame B. */
    for (uint32_t i = 0; i < 10; i++)
    {
        ledFrames[cmd7FrameNdx][i].ledNum = 100 + i;
    }
    RefreshFSMCommand cmd7 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd7FrameNdx,
        .frameLen = 10,
        .dir = NORTH,
    };
    RefreshFSMOutput out8 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR_RANGE, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(50, out8.action.clearRange.startLedNum);
    TEST_ASSERT_EQUAL(2, out8.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd6FrameNdx, out8.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_STANDARD, out8.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out8.framesToRelease.list[1].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out8.framesToRelease.list[1].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out8.isIdle);

    /* (4) starting on the next tick, the FSM outputs REFRESH_ACTION_SET
       actions in the order of frame B until all LEDs are set. The FSM is
       now idle. */
    RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(100, out9.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_FALSE(out9.isIdle);

    for (uint32_t i = 101; i < 100 + 9; i++)
    {
        RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out10.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
        TEST_ASSERT_FALSE(out10.isIdle);
    }

    RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(100 + 9, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_TRUE(out11.isIdle);
}

TEST_CASE("RefreshFSMCommandDuringInstallation", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) NEW_FRAME with the same direction (NORTH) as installed. Clears
       frame (1) entirely; the final REFRESH_ACTION_CLEAR (LED 0) also
       releases frame (1). */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(1, out8.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out8.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out8.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out8.isIdle);

    /* (3) the FSM begins to output REFRESH_ACTION_SET actions in the order
       of frame (2) */
    RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out9.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_FALSE(out9.isIdle);

    RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(21, out10.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_FALSE(out10.isIdle);

    /* (4) while outputting REFRESH_ACTION_SET, REFRESH_CMD_REFRESH is
       sent. A refresh is already effectively occurring, so the FSM
       ignores it: installation continues normally with no state change
       and no difference in output for this tick. */
    RefreshFSMCommand cmd11 = { .type = REFRESH_CMD_REFRESH };
    RefreshFSMOutput out11 = refreshFSMTick(&cmd11);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(22, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_FALSE(out11.isIdle);

    /* the FSM continues outputting REFRESH_ACTION_SET actions until all
       LEDs of frame (2) are installed. The FSM is now idle. */
    for (uint32_t i = 23; i < 20 + 14; i++)
    {
        RefreshFSMOutput out12 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out12.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out12.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out12.framesToRelease.len);
        TEST_ASSERT_FALSE(out12.isIdle);
    }

    RefreshFSMOutput out13 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out13.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20 + 14, out13.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out13.framesToRelease.len);
    TEST_ASSERT_TRUE(out13.isIdle);
}

TEST_CASE("typicalFrameQueueingDuringInstallation", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;
    const uint32_t cmd7FrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) NEW_FRAME with the same direction (NORTH) as installed. Clears
       frame (1) entirely; the final REFRESH_ACTION_CLEAR (LED 0) also
       releases frame (1). */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(1, out8.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out8.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out8.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out8.isIdle);

    /* (3) the FSM begins to output REFRESH_ACTION_SET actions in the order
       of frame (2) */
    RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out9.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_FALSE(out9.isIdle);

    RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(21, out10.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_FALSE(out10.isIdle);

    /* (4) while outputting REFRESH_ACTION_SET, REFRESH_CMD_UPDATE_TYPICAL
       is sent with the same direction (NORTH) as frame (2). The old
       typical frame is still in use for the current frame installation,
       so the FSM only buffers the update; no output or release occurs and
       installation continues normally. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd7FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd11 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd7FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out11 = refreshFSMTick(&cmd11);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(22, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_FALSE(out11.isIdle);

    /* (5) the FSM continues outputting REFRESH_ACTION_SET actions until
       all LEDs of frame (2) are set. The FSM is now idle. */
    for (uint32_t i = 23; i < 20 + 14; i++)
    {
        RefreshFSMOutput out12 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out12.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out12.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out12.framesToRelease.len);
        TEST_ASSERT_FALSE(out12.isIdle);
    }

    RefreshFSMOutput out13 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out13.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20 + 14, out13.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out13.framesToRelease.len);
    TEST_ASSERT_TRUE(out13.isIdle);

    /* the typical frame queued in (4) is still not released or applied
       now that installation is complete -- it stays buffered until the
       next frame installation begins */
    RefreshFSMOutput out14 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out14.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out14.framesToRelease.len);
    TEST_ASSERT_TRUE(out14.isIdle);
}

TEST_CASE("typicalFrameDoubleQueueingDuringInstallation", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;
    const uint32_t cmd7FrameNdx = 4;
    const uint32_t cmd8FrameNdx = 5;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) NEW_FRAME with the same direction (NORTH) as installed. Clears
       frame (1) entirely; the final REFRESH_ACTION_CLEAR (LED 0) also
       releases frame (1). */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(1, out8.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out8.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out8.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out8.isIdle);

    /* (3) the FSM begins to output REFRESH_ACTION_SET actions in the order
       of frame (2) */
    RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out9.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_FALSE(out9.isIdle);

    RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(21, out10.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_FALSE(out10.isIdle);

    /* (4) while outputting REFRESH_ACTION_SET, REFRESH_CMD_UPDATE_TYPICAL
       is sent with the same direction (NORTH) as frame (2). The FSM only
       buffers the update, since the old typical frame is still in use for
       the current installation. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd7FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd11 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd7FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out11 = refreshFSMTick(&cmd11);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(22, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_FALSE(out11.isIdle);

    RefreshFSMOutput out12 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out12.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(23, out12.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out12.framesToRelease.len);
    TEST_ASSERT_FALSE(out12.isIdle);

    /* (5) REFRESH_CMD_UPDATE_TYPICAL is sent again with the same direction
       as (4). The FSM outputs a FRAME_RELEASE_QUEUED_TYPICAL for the old
       queued typical frame of (4), and buffers the new typical frame in
       its place, because the current typical frame is still in use for
       the current frame installation. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd8FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd13 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd8FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out13 = refreshFSMTick(&cmd13);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out13.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(24, out13.action.set.ledNum);
    TEST_ASSERT_EQUAL(1, out13.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd7FrameNdx, out13.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL, out13.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out13.isIdle);

    /* the FSM continues outputting REFRESH_ACTION_SET actions until all
       LEDs of frame (2) are set. The FSM is now idle. */
    for (uint32_t i = 25; i < 20 + 14; i++)
    {
        RefreshFSMOutput out14 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out14.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out14.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out14.framesToRelease.len);
        TEST_ASSERT_FALSE(out14.isIdle);
    }

    RefreshFSMOutput out15 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out15.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20 + 14, out15.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out15.framesToRelease.len);
    TEST_ASSERT_TRUE(out15.isIdle);
}

TEST_CASE("newFrameUpdateDuringInstallation", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;
    const uint32_t cmd7FrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) NEW_FRAME frame A, same direction (NORTH) as installed. Clears
       frame (1) entirely; the final REFRESH_ACTION_CLEAR (LED 0) also
       releases frame (1). */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(1, out8.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out8.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out8.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out8.isIdle);

    /* (3) the FSM begins to output REFRESH_ACTION_SET actions in the order
       of frame (2) */
    RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out9.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_FALSE(out9.isIdle);

    RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(21, out10.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_FALSE(out10.isIdle);

    RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(22, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_FALSE(out11.isIdle);

    /* (4) while outputting REFRESH_ACTION_SET, REFRESH_CMD_NEW_FRAME is
       sent with the same direction as frame (2) (frame C). The FSM stops
       installing frame A and immediately begins clearing it in reverse
       order, starting from the most recently set LED in (3) (ledNum 22,
       the third LED installed). Only the LEDs already installed (22, 21,
       20) need to be cleared. The final REFRESH_ACTION_CLEAR (for the
       first LED of frame A) also releases frame A. */
    for (uint32_t i = 0; i < 8; i++)
    {
        ledFrames[cmd7FrameNdx][i].ledNum = 200 + i;
    }
    RefreshFSMCommand cmd12 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd7FrameNdx,
        .frameLen = 8,
        .dir = NORTH,
    };
    RefreshFSMOutput out12 = refreshFSMTick(&cmd12);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out12.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(22, out12.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out12.framesToRelease.len);
    TEST_ASSERT_FALSE(out12.isIdle);

    RefreshFSMOutput out13 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out13.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(21, out13.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out13.framesToRelease.len);
    TEST_ASSERT_FALSE(out13.isIdle);

    RefreshFSMOutput out14 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out14.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out14.action.clear.ledNum);
    TEST_ASSERT_EQUAL(1, out14.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd6FrameNdx, out14.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out14.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out14.isIdle);

    /* (5) the FSM outputs REFRESH_ACTION_SET in the order of frame C until
       all LEDs are set. The FSM is now idle. */
    RefreshFSMOutput out15 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out15.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(200, out15.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out15.framesToRelease.len);
    TEST_ASSERT_FALSE(out15.isIdle);

    for (uint32_t i = 201; i < 200 + 7; i++)
    {
        RefreshFSMOutput out16 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out16.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out16.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out16.framesToRelease.len);
        TEST_ASSERT_FALSE(out16.isIdle);
    }

    RefreshFSMOutput out17 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out17.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(200 + 7, out17.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out17.framesToRelease.len);
    TEST_ASSERT_TRUE(out17.isIdle);
}

TEST_CASE("ledColorReflectsLiveVsTypicalSpeed", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* typical speed frames are indexed directly by ledNum, so they must be
       full length. Every LED's typical (free-flow) speed is 100, so the
       live speed value doubles as its percentage of typical. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
        ledFrames[cmd2FrameNdx][i].speed = 100;
        ledFrames[cmd3FrameNdx][i].ledNum = i;
        ledFrames[cmd3FrameNdx][i].speed = 100;
    }

    /* current frame: LED 0 is at 90% of typical (fast), LED 1 is at 60% of
       typical (medium), LED 2 is at 30% of typical (slow) */
    ledFrames[cmd1FrameNdx][0].ledNum = 0;
    ledFrames[cmd1FrameNdx][0].speed = 90;
    ledFrames[cmd1FrameNdx][1].ledNum = 1;
    ledFrames[cmd1FrameNdx][1].speed = 60;
    ledFrames[cmd1FrameNdx][2].ledNum = 2;
    ledFrames[cmd1FrameNdx][2].speed = 30;

    /* initialize FSM with distinct, recognizable colors for each speed
       category */
    const Color slowColor = { .red = 0xFF, .green = 0x00, .blue = 0x00 };
    const Color mediumColor = { .red = 0x00, .green = 0xFF, .blue = 0x00 };
    const Color fastColor = { .red = 0x00, .green = 0x00, .blue = 0xFF };
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = slowColor,
        .mediumLEDColor = mediumColor,
        .fastLEDColor = fastColor,
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = 3,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH -- this completes both
       typical frames, so the FSM immediately begins installing frame (1) */
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    /* LED 0 is at 90% of its typical speed, at or above
       CONFIG_MEDIUM_CUTOFF_PERCENT (80), so it should be colored
       fastLEDColor */
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(fastColor.red, out3.action.set.color.red, "LED 0 color.red");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(fastColor.green, out3.action.set.color.green, "LED 0 color.green");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(fastColor.blue, out3.action.set.color.blue, "LED 0 color.blue");
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* LED 1 is at 60% of its typical speed, between
       CONFIG_SLOW_CUTOFF_PERCENT (50) and CONFIG_MEDIUM_CUTOFF_PERCENT
       (80), so it should be colored mediumLEDColor */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(mediumColor.red, out4.action.set.color.red, "LED 1 color.red");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(mediumColor.green, out4.action.set.color.green, "LED 1 color.green");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(mediumColor.blue, out4.action.set.color.blue, "LED 1 color.blue");
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_FALSE(out4.isIdle);

    /* LED 2 is at 30% of its typical speed, below CONFIG_SLOW_CUTOFF_PERCENT
       (50), so it should be colored slowLEDColor. This is the last LED of
       the frame, so the FSM is now idle. */
    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(2, out5.action.set.ledNum);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(slowColor.red, out5.action.set.color.red, "LED 2 color.red");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(slowColor.green, out5.action.set.color.green, "LED 2 color.green");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(slowColor.blue, out5.action.set.color.blue, "LED 2 color.blue");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);
}

TEST_CASE("nightModeEnabledDuringInstallation", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) NEW_FRAME frame A, same direction (NORTH) as installed. Clears
       frame (1) entirely; the final REFRESH_ACTION_CLEAR (LED 0) also
       releases frame (1). */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(1, out8.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out8.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out8.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out8.isIdle);

    /* (3) the FSM begins to output REFRESH_ACTION_SET actions in the order
       of frame (2) */
    RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out9.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_FALSE(out9.isIdle);

    RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(21, out10.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_FALSE(out10.isIdle);

    RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(22, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_FALSE(out11.isIdle);

    /* (4) while outputting REFRESH_ACTION_SET, REFRESH_CMD_NIGHT_MODE_ON is
       sent. Night mode must darken the board regardless of what the FSM is
       doing, so the FSM should stop installing frame (2) and immediately
       start clearing the LEDs already set, in reverse order, starting from
       the most recently set LED (ledNum 22) -- mirroring how a
       REFRESH_CMD_NEW_FRAME sent mid-install interrupts installation (see
       "New Frame Update During Installation"). No frame is released, since
       frame (2) is not being replaced. */
    RefreshFSMCommand cmd12 = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput out12 = refreshFSMTick(&cmd12);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out12.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(22, out12.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out12.framesToRelease.len);
    TEST_ASSERT_FALSE(out12.isIdle);

    RefreshFSMOutput out13 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out13.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(21, out13.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out13.framesToRelease.len);
    TEST_ASSERT_FALSE(out13.isIdle);

    /* (5) once the already-installed LEDs (22, 21, 20) are cleared, since
       night mode is still on, the FSM does not reinstall frame (2) -- the
       board stays dark and the FSM goes idle instead. No frame is
       released, since frame (2) still belongs to the FSM for when night
       mode turns back off. */
    RefreshFSMOutput out14 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out14.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out14.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out14.framesToRelease.len);
    TEST_ASSERT_TRUE(out14.isIdle);

    /* the board remains dark and the FSM remains idle on further
       REFRESH_CMD_NONE ticks -- it does not spontaneously resume
       installing frame (2) */
    RefreshFSMOutput out15 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out15.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out15.framesToRelease.len);
    TEST_ASSERT_TRUE(out15.isIdle);
}

TEST_CASE("multipleFramesQueuedWhileWaitingForFrames", TEST_GROUP)
{
    const uint32_t frameAFrameNdx = 0;
    const uint32_t frameBFrameNdx = 1;
    const uint32_t frameCFrameNdx = 2;
    const uint32_t typicalNorthFrameNdx = 3;
    const uint32_t typicalSouthFrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* (1) send REFRESH_CMD_NEW_FRAME for frame A. The FSM has no current
       frame yet, so frame A is latched directly as the current frame with
       no output or release. The FSM remains idle: it still has no typical
       data. */
    for (uint32_t i = 0; i < 5; i++)
    {
        ledFrames[frameAFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdA = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameAFrameNdx,
        .frameLen = 5,
        .dir = NORTH,
    };
    RefreshFSMOutput outA = refreshFSMTick(&cmdA);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outA.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outA.framesToRelease.len);
    TEST_ASSERT_TRUE(outA.isIdle);

    /* (2) send REFRESH_CMD_NEW_FRAME for frame B. A current frame (A)
       already exists, so frame B is only buffered as the next frame to
       install. Nothing was queued before, so no release occurs. */
    for (uint32_t i = 0; i < 5; i++)
    {
        ledFrames[frameBFrameNdx][i].ledNum = 10 + i;
    }
    RefreshFSMCommand cmdB = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameBFrameNdx,
        .frameLen = 5,
        .dir = NORTH,
    };
    RefreshFSMOutput outB = refreshFSMTick(&cmdB);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outB.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outB.framesToRelease.len);
    TEST_ASSERT_TRUE(outB.isIdle);

    /* (3) send REFRESH_CMD_NEW_FRAME for frame C. Frame B is already
       queued as the next frame, so the FSM immediately releases frame B
       as FRAME_RELEASE_QUEUED_STANDARD -- it is displaced before it was
       ever installed -- and buffers frame C in its place. */
    for (uint32_t i = 0; i < 5; i++)
    {
        ledFrames[frameCFrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmdC = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameCFrameNdx,
        .frameLen = 5,
        .dir = NORTH,
    };
    RefreshFSMOutput outC = refreshFSMTick(&cmdC);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outC.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, outC.framesToRelease.len);
    TEST_ASSERT_EQUAL(frameBFrameNdx, outC.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_STANDARD, outC.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_TRUE(outC.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH -- still missing SOUTH, so
       the FSM remains idle with no output or release */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalNorthFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdTypicalNorth = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalNorthFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput outTypicalNorth = refreshFSMTick(&cmdTypicalNorth);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outTypicalNorth.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outTypicalNorth.framesToRelease.len);
    TEST_ASSERT_TRUE(outTypicalNorth.isIdle);

    /* (4) send REFRESH_CMD_UPDATE_TYPICAL for SOUTH. Both directions now
       have typical data, so the FSM releases frame A as
       FRAME_RELEASE_STANDARD -- frame A was never displayed, only held as
       the current frame while queueing occurred -- and begins installing
       frame C, the frame left queued after (3). Frame B is never
       installed and is not released again; it was already accounted for
       in (3). */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalSouthFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdTypicalSouth = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalSouthFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput outTypicalSouth = refreshFSMTick(&cmdTypicalSouth);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outTypicalSouth.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, outTypicalSouth.action.set.ledNum);
    TEST_ASSERT_EQUAL(1, outTypicalSouth.framesToRelease.len);
    TEST_ASSERT_EQUAL(frameAFrameNdx, outTypicalSouth.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, outTypicalSouth.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(outTypicalSouth.isIdle);

    /* the FSM installs frame C -- and only frame C -- to completion.
       Frames A and B never appear as REFRESH_ACTION_SET LEDs. */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 21; i < 24; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(24, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("nightModeRoundTripRetainsFrameOwnership", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) REFRESH_CMD_NIGHT_MODE_ON is sent. The FSM begins clearing frame
       (1) from the board, one LED per tick, in reverse order. No frame is
       released -- frame (1) is only being hidden, not replaced. */
    RefreshFSMCommand cmd6 = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    /* (3) once fully cleared, because night mode is still on, the FSM goes
       idle with the board dark instead of reinstalling frame (1). No
       release occurs -- frame (1) is still owned by the FSM, waiting to be
       redisplayed once night mode turns off. */
    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_TRUE(out8.isIdle);

    /* the board stays dark and the FSM remains idle on further
       REFRESH_CMD_NONE ticks */
    RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_TRUE(out9.isIdle);

    /* (4) REFRESH_CMD_NIGHT_MODE_OFF is sent. No other frame or typical
       update was ever queued while dark, so the FSM reinstalls frame (1)
       -- the same frame it has held onto the entire time -- starting from
       its first LED. No frame is released during reinstallation, since
       frame (1) was never replaced. */
    RefreshFSMCommand cmd10 = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput out10 = refreshFSMTick(&cmd10);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out10.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_FALSE(out10.isIdle);

    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out11.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
        TEST_ASSERT_FALSE(out11.isIdle);
    }

    /* frame (1) and both typical frames were never released or
       reallocated throughout the entire night mode round trip -- only the
       board's on/off display state changed */
    RefreshFSMOutput out12 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out12.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out12.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out12.framesToRelease.len);
    TEST_ASSERT_TRUE(out12.isIdle);
}

TEST_CASE("typicalFrameDoubleQueueingDuringClearing", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t cmd6FrameNdx = 3;
    const uint32_t cmd7FrameNdx = 4;
    const uint32_t cmd8FrameNdx = 5;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) NEW_FRAME with the same direction (NORTH) as installed. The FSM
       queues the frame and begins clearing the board in reverse order. */
    for (uint32_t i = 0; i < 15; i++)
    {
        ledFrames[cmd6FrameNdx][i].ledNum = 20 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd6FrameNdx,
        .frameLen = 15,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    /* send REFRESH_CMD_NONE to continue clearing partway through the board */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 10; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    /* (3) while REFRESH_ACTION_CLEAR is still being output, send
       REFRESH_CMD_UPDATE_TYPICAL for typical frame A, same direction
       (NORTH). Nothing was previously queued for NORTH, so the FSM only
       buffers typical frame A -- no release, and clearing continues
       uninterrupted in the same tick. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd7FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd7 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd7FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out8 = refreshFSMTick(&cmd7);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(10, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_FALSE(out8.isIdle);

    /* (4) immediately after, REFRESH_CMD_UPDATE_TYPICAL is sent again for
       NORTH, with typical frame B. Typical frame A is already queued, so
       the FSM immediately releases it with a FRAME_RELEASE_QUEUED_TYPICAL
       -- it is displaced before ever being latched in or used -- and
       buffers typical frame B in its place. Clearing continues
       uninterrupted in the same tick. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd8FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd9 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd8FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out9 = refreshFSMTick(&cmd9);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(9, out9.action.clear.ledNum);
    TEST_ASSERT_EQUAL(1, out9.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd7FrameNdx, out9.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL, out9.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out9.isIdle);

    /* continue clearing down to LED 1 */
    for (uint32_t i = 8; i > 0; i--)
    {
        RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out10.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out10.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
        TEST_ASSERT_FALSE(out10.isIdle);
    }

    /* (5) the final REFRESH_ACTION_CLEAR, for LED 0 (the first LED of
       frame (1)), releases the old standard frame of (1) and the old
       NORTH typical frame in the same output. Typical frame B (queued in
       (4)) is latched in as the new current NORTH typical frame -- it is
       not released here, since it is now in active use. */
    RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out11.action.clear.ledNum);
    TEST_ASSERT_EQUAL(2, out11.framesToRelease.len);
    if (cmd1FrameNdx == out11.framesToRelease.list[0].index)
    {
        TEST_ASSERT_EQUAL(cmd1FrameNdx, out11.framesToRelease.list[0].index);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out11.framesToRelease.list[0].type, "refresh frame release type");
        TEST_ASSERT_EQUAL(cmd2FrameNdx, out11.framesToRelease.list[1].index);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_TYPICAL_NORTH, out11.framesToRelease.list[1].type, "refresh frame release type");
    } else
    {
        TEST_ASSERT_EQUAL(cmd2FrameNdx, out11.framesToRelease.list[0].index);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_TYPICAL_NORTH, out11.framesToRelease.list[0].type, "refresh frame release type");
        TEST_ASSERT_EQUAL(cmd1FrameNdx, out11.framesToRelease.list[1].index);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out11.framesToRelease.list[1].type, "refresh frame release type");
    }
    TEST_ASSERT_FALSE(out11.isIdle);

    /* (6) the FSM outputs REFRESH_ACTION_SET actions in the same order of
       frame (2) until all LEDs of the frame are set. The FSM is now idle.
       Frame (2) and typical frame B are never released -- they are the
       frame and typical frame now actively in use. */
    RefreshFSMOutput out12 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out12.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20, out12.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out12.framesToRelease.len);
    TEST_ASSERT_FALSE(out12.isIdle);

    for (uint32_t i = 21; i < 20 + 14; i++)
    {
        RefreshFSMOutput out13 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out13.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out13.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out13.framesToRelease.len);
        TEST_ASSERT_FALSE(out13.isIdle);
    }

    RefreshFSMOutput out14 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out14.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(20 + 14, out14.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out14.framesToRelease.len);
    TEST_ASSERT_TRUE(out14.isIdle);
}

TEST_CASE("independentTypicalFrameQueuesPerDirection", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t typicalAFrameNdx = 3;
    const uint32_t typicalBFrameNdx = 4;
    const uint32_t typicalCFrameNdx = 5;
    const uint32_t typicalDFrameNdx = 6;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) REFRESH_CMD_UPDATE_TYPICAL is sent for NORTH (typical A), then
       for SOUTH (typical B). Neither direction had a queued update
       pending, so both are only buffered -- no release for either. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalAFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdA = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalAFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput outA = refreshFSMTick(&cmdA);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outA.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outA.framesToRelease.len);
    TEST_ASSERT_TRUE(outA.isIdle);

    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalBFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdB = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalBFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput outB = refreshFSMTick(&cmdB);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outB.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outB.framesToRelease.len);
    TEST_ASSERT_TRUE(outB.isIdle);

    /* (3) REFRESH_CMD_UPDATE_TYPICAL is sent again for NORTH (typical C).
       Typical A is already queued for NORTH, so the FSM releases it as
       FRAME_RELEASE_QUEUED_TYPICAL and buffers typical C in its place.
       Typical B, still queued for SOUTH, is unaffected. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalCFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdC = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalCFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput outC = refreshFSMTick(&cmdC);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outC.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, outC.framesToRelease.len);
    TEST_ASSERT_EQUAL(typicalAFrameNdx, outC.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL, outC.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_TRUE(outC.isIdle);

    /* (4) REFRESH_CMD_UPDATE_TYPICAL is sent again for SOUTH (typical D).
       Typical B is already queued for SOUTH, so the FSM releases it as
       FRAME_RELEASE_QUEUED_TYPICAL and buffers typical D in its place.
       Typical C, still queued for NORTH, is unaffected. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalDFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdD = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalDFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput outD = refreshFSMTick(&cmdD);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outD.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, outD.framesToRelease.len);
    TEST_ASSERT_EQUAL(typicalBFrameNdx, outD.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL, outD.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_TRUE(outD.isIdle);

    /* (5) REFRESH_CMD_REFRESH is sent. No frame was ever queued, so no
       frame is released; the FSM clears then reinstalls the current
       frame. */
    RefreshFSMCommand cmdRefresh = { .type = REFRESH_CMD_REFRESH };
    RefreshFSMOutput outRefresh = refreshFSMTick(&cmdRefresh);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, outRefresh.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outRefresh.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, outRefresh.framesToRelease.len);
    TEST_ASSERT_FALSE(outRefresh.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    /* the final REFRESH_ACTION_CLEAR releases the original NORTH typical
       (cmd2) with FRAME_RELEASE_TYPICAL_NORTH and the original SOUTH
       typical (cmd3) with FRAME_RELEASE_TYPICAL_SOUTH, in that order --
       the frame and typical release checks in transitionToInstallingFrame
       run sequentially (frame, then NORTH, then SOUTH), so this ordering
       is deterministic. Typicals C and D are latched in as the new
       current typicals and are not released here. */
    RefreshFSMOutput outFinalClear = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, outFinalClear.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outFinalClear.action.clear.ledNum);
    TEST_ASSERT_EQUAL(2, outFinalClear.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd2FrameNdx, outFinalClear.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_TYPICAL_NORTH, outFinalClear.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_EQUAL(cmd3FrameNdx, outFinalClear.framesToRelease.list[1].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_TYPICAL_SOUTH, outFinalClear.framesToRelease.list[1].type, "refresh frame release type");
    TEST_ASSERT_FALSE(outFinalClear.isIdle);

    /* the FSM reinstalls the current frame (unchanged) and goes idle.
       Typicals C and D are never released -- they are now in active
       use. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outFinalSet = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outFinalSet.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outFinalSet.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outFinalSet.framesToRelease.len);
    TEST_ASSERT_TRUE(outFinalSet.isIdle);
}

TEST_CASE("typicalFrameDoubleQueueingWhileWaitingForFrames", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t typicalAFrameNdx = 1;
    const uint32_t typicalBFrameNdx = 2;
    const uint32_t typicalCFrameNdx = 3;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* (1) send REFRESH_CMD_NEW_FRAME for frame (1). The FSM has no current
       frame yet, so frame (1) is latched directly as the current frame
       with no output or release. The FSM remains idle. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* (2) send REFRESH_CMD_UPDATE_TYPICAL for NORTH (typical A). Nothing
       was previously queued for NORTH, so typical A is only buffered --
       no release. The FSM remains idle, still missing SOUTH typical
       data. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalAFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdA = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalAFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput outA = refreshFSMTick(&cmdA);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outA.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outA.framesToRelease.len);
    TEST_ASSERT_TRUE(outA.isIdle);

    /* (3) send REFRESH_CMD_UPDATE_TYPICAL again for NORTH (typical B).
       Typical A is already queued for NORTH, so the FSM releases it with
       a FRAME_RELEASE_QUEUED_TYPICAL and buffers typical B in its place --
       typical A is never latched in or used for a single LED's color. The
       FSM remains idle, still missing SOUTH typical data. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalBFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdB = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalBFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput outB = refreshFSMTick(&cmdB);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outB.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, outB.framesToRelease.len);
    TEST_ASSERT_EQUAL(typicalAFrameNdx, outB.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL, outB.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_TRUE(outB.isIdle);

    /* (4) send REFRESH_CMD_UPDATE_TYPICAL for SOUTH (typical C). Both
       directions now have typical data and a current frame exists, so the
       FSM begins installing frame (1). Frame (1) was latched directly in
       (1) rather than queued, so no frame is released. Typicals B and C
       are each the first ever queued for their direction, so neither
       displaces a current typical -- no typical is released here either.
       The only release across this entire sequence was typical A's
       FRAME_RELEASE_QUEUED_TYPICAL in (3). */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalCFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdC = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalCFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput outC = refreshFSMTick(&cmdC);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outC.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outC.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outC.framesToRelease.len);
    TEST_ASSERT_FALSE(outC.isIdle);

    /* the FSM installs frame (1) to completion with no further
       releases */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("newFrameQueuedWhileBoardIsDark", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t frameBFrameNdx = 3;
    const uint32_t frameCFrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) REFRESH_CMD_NIGHT_MODE_ON is sent. The FSM clears frame (1) from
       the board and, once fully cleared, goes idle with the board dark.
       No frame is released. */
    RefreshFSMCommand cmd6 = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_TRUE(out8.isIdle);

    /* (3) REFRESH_CMD_NEW_FRAME is sent for frame (2), while the board is
       still dark. Because night mode is on, the FSM does not begin
       installing frame (2) -- it is only buffered as the next frame. No
       output is produced and no release occurs. */
    for (uint32_t i = 0; i < 10; i++)
    {
        ledFrames[frameBFrameNdx][i].ledNum = 100 + i;
    }
    RefreshFSMCommand cmd9 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameBFrameNdx,
        .frameLen = 10,
        .dir = NORTH,
    };
    RefreshFSMOutput out9 = refreshFSMTick(&cmd9);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_TRUE(out9.isIdle);

    /* (4) REFRESH_CMD_NEW_FRAME is sent again, for frame (3), still while
       the board is dark. Frame (2) is already queued, so the FSM
       immediately releases it with a FRAME_RELEASE_QUEUED_STANDARD --
       displaced before ever being installed -- and buffers frame (3) in
       its place. The board remains dark. */
    for (uint32_t i = 0; i < 8; i++)
    {
        ledFrames[frameCFrameNdx][i].ledNum = 200 + i;
    }
    RefreshFSMCommand cmd10 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameCFrameNdx,
        .frameLen = 8,
        .dir = NORTH,
    };
    RefreshFSMOutput out10 = refreshFSMTick(&cmd10);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, out10.framesToRelease.len);
    TEST_ASSERT_EQUAL(frameBFrameNdx, out10.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_STANDARD, out10.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_TRUE(out10.isIdle);

    /* (5) REFRESH_CMD_NIGHT_MODE_OFF is sent. Frame (3) is queued, so the
       FSM releases frame (1) with a FRAME_RELEASE_STANDARD -- it is never
       redisplayed, only replaced by the frame queued while dark -- and
       begins installing frame (3) instead. No typical frame is released,
       since neither typical was ever requeued while dark. */
    RefreshFSMCommand cmd11 = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput out11 = refreshFSMTick(&cmd11);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(200, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(1, out11.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out11.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out11.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out11.isIdle);

    /* the FSM installs frame (3) to completion with no further
       releases */
    for (uint32_t i = 201; i < 200 + 7; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(200 + 7, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("frameQueueDisplacementAcrossADirectionChange", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t frameAFrameNdx = 3;
    const uint32_t frameBFrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }

    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) NEW_FRAME for frame A, with a *different* direction (SOUTH) than
       is currently installed (NORTH). The FSM queues frame A and begins
       to output REFRESH_ACTION_CLEAR actions in reverse order of frame
       (1). */
    for (uint32_t i = 0; i < 10; i++)
    {
        ledFrames[frameAFrameNdx][i].ledNum = 50 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameAFrameNdx,
        .frameLen = 10,
        .dir = SOUTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    /* clear partway through the board, stopping well before frame (1) has
       finished clearing */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i >= 50; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    /* (3) while REFRESH_ACTION_CLEAR is still being output, REFRESH_CMD_NEW_FRAME
       is sent again for frame B, with yet another direction (NORTH). A
       frame is already queued (frame A), so the FSM immediately releases
       frame A as FRAME_RELEASE_QUEUED_STANDARD and frame (1) as
       FRAME_RELEASE_STANDARD, in the same tick, and outputs a single
       REFRESH_ACTION_CLEAR_RANGE that clears the remainder of the board
       and immediately begins installing frame B -- the frame queue holds
       only one slot regardless of direction. */
    for (uint32_t i = 0; i < 12; i++)
    {
        ledFrames[frameBFrameNdx][i].ledNum = 300 + i;
    }
    RefreshFSMCommand cmd8 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameBFrameNdx,
        .frameLen = 12,
        .dir = NORTH,
    };
    RefreshFSMOutput out8 = refreshFSMTick(&cmd8);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR_RANGE, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(50, out8.action.clearRange.startLedNum);
    TEST_ASSERT_EQUAL(2, out8.framesToRelease.len);
    TEST_ASSERT_EQUAL(frameAFrameNdx, out8.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_STANDARD, out8.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out8.framesToRelease.list[1].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out8.framesToRelease.list[1].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out8.isIdle);

    /* (4) starting on the next tick, the FSM outputs REFRESH_ACTION_SET
       actions in the order of frame B until all LEDs are set. The FSM is
       now idle. */
    RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(300, out9.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_FALSE(out9.isIdle);

    for (uint32_t i = 301; i < 300 + 11; i++)
    {
        RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out10.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
        TEST_ASSERT_FALSE(out10.isIdle);
    }

    RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(300 + 11, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_TRUE(out11.isIdle);
}

TEST_CASE("interleavedFrameAndTypicalQueueingWhileWaitingForFrames", TEST_GROUP)
{
    const uint32_t frameAFrameNdx = 0;
    const uint32_t typicalAFrameNdx = 1;
    const uint32_t frameBFrameNdx = 2;
    const uint32_t typicalCFrameNdx = 3;
    const uint32_t frameCFrameNdx = 4;
    const uint32_t typicalDFrameNdx = 5;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* (1) send REFRESH_CMD_NEW_FRAME for frame A. The FSM has no current
       frame yet, so frame A is latched directly as the current frame with
       no output or release. */
    for (uint32_t i = 0; i < 5; i++)
    {
        ledFrames[frameAFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdFrameA = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameAFrameNdx,
        .frameLen = 5,
        .dir = NORTH,
    };
    RefreshFSMOutput outFrameA = refreshFSMTick(&cmdFrameA);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outFrameA.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outFrameA.framesToRelease.len);
    TEST_ASSERT_TRUE(outFrameA.isIdle);

    /* (2) send REFRESH_CMD_UPDATE_TYPICAL for NORTH (typical A). Nothing
       was previously queued for NORTH, so typical A is only buffered. The
       FSM remains idle, still missing SOUTH typical data. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalAFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdTypicalA = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalAFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput outTypicalA = refreshFSMTick(&cmdTypicalA);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outTypicalA.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outTypicalA.framesToRelease.len);
    TEST_ASSERT_TRUE(outTypicalA.isIdle);

    /* (3) send REFRESH_CMD_NEW_FRAME for frame B. A current frame already
       exists (frame A), so frame B is only buffered as the next frame to
       install. No release occurs yet. */
    for (uint32_t i = 0; i < 5; i++)
    {
        ledFrames[frameBFrameNdx][i].ledNum = 500 + i;
    }
    RefreshFSMCommand cmdFrameB = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameBFrameNdx,
        .frameLen = 5,
        .dir = NORTH,
    };
    RefreshFSMOutput outFrameB = refreshFSMTick(&cmdFrameB);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outFrameB.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outFrameB.framesToRelease.len);
    TEST_ASSERT_TRUE(outFrameB.isIdle);

    /* (4) send REFRESH_CMD_UPDATE_TYPICAL again for NORTH (typical C).
       Typical A is already queued for NORTH, so the FSM releases it as
       FRAME_RELEASE_QUEUED_TYPICAL and buffers typical C in its place.
       Frame B, queued in (3), is unaffected. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalCFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdTypicalC = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalCFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput outTypicalC = refreshFSMTick(&cmdTypicalC);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outTypicalC.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, outTypicalC.framesToRelease.len);
    TEST_ASSERT_EQUAL(typicalAFrameNdx, outTypicalC.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL, outTypicalC.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_TRUE(outTypicalC.isIdle);

    /* (5) send REFRESH_CMD_NEW_FRAME again for frame C. Frame B is
       already queued, so the FSM releases it as
       FRAME_RELEASE_QUEUED_STANDARD and buffers frame C in its place.
       Typical C, queued in (4), is unaffected. */
    for (uint32_t i = 0; i < 6; i++)
    {
        ledFrames[frameCFrameNdx][i].ledNum = 400 + i;
    }
    RefreshFSMCommand cmdFrameC = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameCFrameNdx,
        .frameLen = 6,
        .dir = NORTH,
    };
    RefreshFSMOutput outFrameC = refreshFSMTick(&cmdFrameC);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outFrameC.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, outFrameC.framesToRelease.len);
    TEST_ASSERT_EQUAL(frameBFrameNdx, outFrameC.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_STANDARD, outFrameC.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_TRUE(outFrameC.isIdle);

    /* (6) send REFRESH_CMD_UPDATE_TYPICAL for SOUTH (typical D). Nothing
       was previously queued for SOUTH, so typical D is only buffered --
       but this also completes both directions' typical data, so the FSM
       immediately begins installing frame C (the frame left queued after
       (5)). Frame A is released as FRAME_RELEASE_STANDARD, since it was
       only ever held as the current frame while queueing occurred, never
       displayed. Neither typical C nor typical D displaces a current
       typical -- both are the first ever latched for their direction --
       so no typical is released here. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalDFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmdTypicalD = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalDFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput outTypicalD = refreshFSMTick(&cmdTypicalD);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outTypicalD.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(400, outTypicalD.action.set.ledNum);
    TEST_ASSERT_EQUAL(1, outTypicalD.framesToRelease.len);
    TEST_ASSERT_EQUAL(frameAFrameNdx, outTypicalD.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, outTypicalD.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(outTypicalD.isIdle);

    /* the FSM installs frame C to completion with no further releases --
       frame C, typical C, and typical D are now in active use and are
       never released */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 401; i < 400 + 5; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(400 + 5, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("nightModeEnabledWhileWaitingForFrames", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* (1) REFRESH_CMD_NIGHT_MODE_ON is sent before anything else has
       arrived. Nothing is displayed yet, so no output occurs and the FSM
       remains idle. */
    RefreshFSMCommand cmdNightOn = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput outNightOn = refreshFSMTick(&cmdNightOn);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outNightOn.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outNightOn.framesToRelease.len);
    TEST_ASSERT_TRUE(outNightOn.isIdle);

    /* (2) REFRESH_CMD_NEW_FRAME is sent for frame (1). The FSM has no
       current frame yet, so frame (1) is latched directly with no output
       or release. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* (3) REFRESH_CMD_UPDATE_TYPICAL is sent for SOUTH. Both directions
       now have typical data and a current frame exists, so the FSM has
       everything it needs to begin installing frame (1) -- but because
       night mode is on, it must not light the board. Ownership of frame
       (1) and both typical frames is still fully consumed (no release,
       since all three are being latched in for the first time), but no
       REFRESH_ACTION_SET actions are output and the FSM remains idle with
       the board dark. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_TRUE(out3.isIdle);

    /* the board stays dark and the FSM remains idle on further
       REFRESH_CMD_NONE ticks */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    RefreshFSMOutput outStillDark = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outStillDark.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outStillDark.framesToRelease.len);
    TEST_ASSERT_TRUE(outStillDark.isIdle);

    /* (4) REFRESH_CMD_NIGHT_MODE_OFF is sent. No other frame or typical
       update was ever queued while dark, so the FSM installs frame (1)
       one LED per tick, in its original order, with no release. */
    RefreshFSMCommand cmdNightOff = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput outNightOff = refreshFSMTick(&cmdNightOff);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outNightOff.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outNightOff.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outNightOff.framesToRelease.len);
    TEST_ASSERT_FALSE(outNightOff.isIdle);

    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("nightModeOffCancelsAPendingClear", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) REFRESH_CMD_NIGHT_MODE_ON is sent. The FSM begins clearing
       frame (1) from the board, one LED per tick, in reverse order. */
    RefreshFSMCommand cmd6 = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    /* clear partway through the board, stopping before frame (1) has
       finished clearing */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 10; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    /* (3) while REFRESH_ACTION_CLEAR is still being output, before frame
       (1) has finished clearing, REFRESH_CMD_NIGHT_MODE_OFF is sent. The
       board is not yet REFRESH_FSM_CLEARED, so this has no immediate
       effect beyond clearing the night mode flag -- clearing continues
       normally, uninterrupted, in the same tick. */
    RefreshFSMCommand cmd8 = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput out8 = refreshFSMTick(&cmd8);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(10, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_FALSE(out8.isIdle);

    /* continue clearing down to LED 1 */
    for (uint32_t i = 9; i > 0; i--)
    {
        RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out9.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out9.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
        TEST_ASSERT_FALSE(out9.isIdle);
    }

    /* (4) once frame (1) is fully cleared (LED 0), the FSM re-checks the
       night mode flag at that moment -- it is now off, so the FSM does
       not go dark. It does not yet reinstall on this same tick (the
       action stays REFRESH_ACTION_CLEAR for LED 0); reinstallation begins
       on the following tick, exactly like an ordinary REFRESH_CMD_REFRESH
       or REFRESH_CMD_NEW_FRAME completion. No release occurs, since
       nothing was ever replaced. */
    RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out10.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_FALSE(out10.isIdle);

    /* the FSM reinstalls frame (1) -- the same frame it held onto the
       entire time -- one LED per tick, in its original order, with no
       release throughout */
    RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_FALSE(out11.isIdle);

    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("nightModeOffIsANoOpWhenTheBoardIsNotDark", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) REFRESH_CMD_NIGHT_MODE_OFF is sent. Night mode was never turned
       on and the board is not REFRESH_FSM_CLEARED, so this has no effect
       beyond confirming the (already-off) night mode flag. No output is
       produced, no release occurs, and the FSM remains idle with frame
       (1) undisturbed. */
    RefreshFSMCommand cmd6 = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_TRUE(out6.isIdle);

    /* the FSM remains idle, still showing frame (1), on further
       REFRESH_CMD_NONE ticks */
    RefreshFSMOutput out7 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out7.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
    TEST_ASSERT_TRUE(out7.isIdle);
}

TEST_CASE("nightModeOnDuringAPendingClear", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t frame2FrameNdx = 3;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) NEW_FRAME for frame (2), same direction (NORTH) as installed.
       The FSM queues frame (2) and begins clearing frame (1) from the
       board, one LED per tick, in reverse order. */
    for (uint32_t i = 0; i < 10; i++)
    {
        ledFrames[frame2FrameNdx][i].ledNum = 100 + i;
    }
    RefreshFSMCommand cmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frame2FrameNdx,
        .frameLen = 10,
        .dir = NORTH,
    };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    /* clear partway through the board, stopping before frame (1) has
       finished clearing */
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 10; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    /* (3) while REFRESH_ACTION_CLEAR is still being output, before frame
       (1) has finished clearing, REFRESH_CMD_NIGHT_MODE_ON is sent. This
       has no immediate effect beyond setting the night mode flag --
       clearing continues normally, uninterrupted, in the same tick. */
    RefreshFSMCommand cmd8 = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput out8 = refreshFSMTick(&cmd8);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(10, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_FALSE(out8.isIdle);

    /* continue clearing down to LED 1 */
    for (uint32_t i = 9; i > 0; i--)
    {
        RefreshFSMOutput out9 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out9.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out9.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
        TEST_ASSERT_FALSE(out9.isIdle);
    }

    /* (4) once frame (1) is fully cleared (LED 0), the FSM checks the
       night mode flag: it is on, so instead of installing frame (2), the
       FSM goes idle with the board dark. Neither frame (1) nor frame (2)
       is released -- frame (2) remains safely queued, and frame (1),
       though no longer displayed, has not yet been replaced. Note that
       isIdle is true on this exact tick (unlike an ordinary clear-then-
       reinstall completion, where isIdle stays false until installation
       actually begins on a later tick). */
    RefreshFSMOutput out10 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out10.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_TRUE(out10.isIdle);

    /* the board stays dark and the FSM remains idle on further
       REFRESH_CMD_NONE ticks */
    RefreshFSMOutput out11 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out11.framesToRelease.len);
    TEST_ASSERT_TRUE(out11.isIdle);

    /* (5) REFRESH_CMD_NIGHT_MODE_OFF is sent. Frame (2) is queued as the
       next frame, so the FSM releases frame (1) with a
       FRAME_RELEASE_STANDARD -- it is never redisplayed, only replaced by
       the frame that was queued before the board went dark -- and
       installs frame (2) instead. */
    RefreshFSMCommand cmd12 = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput out12 = refreshFSMTick(&cmd12);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out12.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(100, out12.action.set.ledNum);
    TEST_ASSERT_EQUAL(1, out12.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, out12.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, out12.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out12.isIdle);

    /* the FSM installs frame (2) to completion with no further releases */
    for (uint32_t i = 101; i < 100 + 9; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(100 + 9, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("typicalFrameUpdateQueuedWhileBoardIsDark", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t typicalAFrameNdx = 3;
    const uint32_t typicalBFrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) REFRESH_CMD_NIGHT_MODE_ON is sent. The FSM clears frame (1) from
       the board and, once fully cleared, goes idle with the board dark.
       No frame or typical frame is released. */
    RefreshFSMCommand cmd6 = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_TRUE(out8.isIdle);

    /* (3) REFRESH_CMD_UPDATE_TYPICAL is sent for NORTH (typical A), while
       the board is still dark. Nothing was previously queued for NORTH,
       so typical A is only buffered -- no output, no release. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalAFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd9 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalAFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out9 = refreshFSMTick(&cmd9);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_TRUE(out9.isIdle);

    /* (4) REFRESH_CMD_UPDATE_TYPICAL is sent again for NORTH (typical B),
       still while dark. Typical A is already queued for NORTH, so the FSM
       releases it as FRAME_RELEASE_QUEUED_TYPICAL -- displaced before ever
       being latched in or used -- and buffers typical B in its place. */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[typicalBFrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd10 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = typicalBFrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out10 = refreshFSMTick(&cmd10);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(1, out10.framesToRelease.len);
    TEST_ASSERT_EQUAL(typicalAFrameNdx, out10.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL, out10.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_TRUE(out10.isIdle);

    /* (5) REFRESH_CMD_NIGHT_MODE_OFF is sent. Frame (1) was never
       displaced, so it is reinstalled unchanged and no frame is released.
       Typical B is queued for NORTH, so the FSM releases the original
       NORTH typical frame (from before night mode was ever turned on)
       with a FRAME_RELEASE_TYPICAL_NORTH and latches typical B in its
       place. The SOUTH typical frame was never touched while dark, so it
       is untouched and not released. */
    RefreshFSMCommand cmd11 = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput out11 = refreshFSMTick(&cmd11);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out11.action.set.ledNum);
    TEST_ASSERT_EQUAL(1, out11.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd2FrameNdx, out11.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_TYPICAL_NORTH, out11.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(out11.isIdle);

    /* the FSM installs frame (1) to completion with no further releases */
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast2 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outLast2.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast2.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast2.isIdle);
}

TEST_CASE("RefreshFSMCommandWhileWaitingForFrames", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* (1) REFRESH_CMD_REFRESH is sent before anything else has arrived.
       There is nothing to clear or reinstall, so this has no effect. */
    RefreshFSMCommand cmdRefresh = { .type = REFRESH_CMD_REFRESH };
    RefreshFSMOutput outRefresh = refreshFSMTick(&cmdRefresh);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outRefresh.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outRefresh.framesToRelease.len);
    TEST_ASSERT_TRUE(outRefresh.isIdle);

    /* (2) the FSM bootstraps normally, exactly as if the
       REFRESH_CMD_REFRESH in (1) had never been sent */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("RefreshFSMCommandWhileBoardIsDark", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) REFRESH_CMD_NIGHT_MODE_ON is sent. The FSM clears frame (1) from
       the board and, once fully cleared, goes idle with the board dark.
       No frame is released. */
    RefreshFSMCommand cmd6 = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_TRUE(out8.isIdle);

    /* (3) REFRESH_CMD_REFRESH is sent while the board is still dark.
       Because night mode is on, the FSM does not reinstall -- no output,
       no release, and the board stays dark. */
    RefreshFSMCommand cmd9 = { .type = REFRESH_CMD_REFRESH };
    RefreshFSMOutput out9 = refreshFSMTick(&cmd9);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_TRUE(out9.isIdle);

    /* (4) REFRESH_CMD_NIGHT_MODE_OFF is sent. Frame (1) was never
       displaced by the REFRESH_CMD_REFRESH in (3), so it is reinstalled
       unchanged and no frame is released. */
    RefreshFSMCommand cmd10 = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput out10 = refreshFSMTick(&cmd10);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out10.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_FALSE(out10.isIdle);

    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("nightModeOnWhileAlreadyDarkIsIdempotent", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand cmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
        TEST_ASSERT_FALSE(out4.isIdle);
    }

    RefreshFSMOutput out4 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out4.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out4.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out4.framesToRelease.len);
    TEST_ASSERT_TRUE(out4.isIdle);

    RefreshFSMOutput out5 = refreshFSMTick(&cmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out5.framesToRelease.len);
    TEST_ASSERT_TRUE(out5.isIdle);

    /* (2) REFRESH_CMD_NIGHT_MODE_ON is sent. The FSM clears frame (1) from
       the board and, once fully cleared, goes idle with the board dark.
       No frame is released. */
    RefreshFSMCommand cmd6 = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput out6 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, out6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out6.framesToRelease.len);
    TEST_ASSERT_FALSE(out6.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i > 0; i--)
    {
        RefreshFSMOutput out7 = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, out7.framesToRelease.len);
        TEST_ASSERT_FALSE(out7.isIdle);
    }

    RefreshFSMOutput out8 = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, out8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, out8.framesToRelease.len);
    TEST_ASSERT_TRUE(out8.isIdle);

    /* (3) REFRESH_CMD_NIGHT_MODE_ON is sent again, while the board is
       already dark. Night mode was already on, so this only re-confirms
       the flag: no output, no release, board stays dark. */
    RefreshFSMOutput out9 = refreshFSMTick(&cmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out9.framesToRelease.len);
    TEST_ASSERT_TRUE(out9.isIdle);

    /* (4) REFRESH_CMD_NIGHT_MODE_OFF is sent. Frame (1) was never
       displaced by the redundant REFRESH_CMD_NIGHT_MODE_ON in (3), so it
       is reinstalled unchanged and no frame is released. */
    RefreshFSMCommand cmd10 = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput out10 = refreshFSMTick(&cmd10);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out10.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out10.framesToRelease.len);
    TEST_ASSERT_FALSE(out10.isIdle);

    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("nightModeToggledOffBeforeDataEverArrives", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* (1) REFRESH_CMD_NIGHT_MODE_ON is sent before anything else has
       arrived. Nothing is displayed yet, so no output occurs. */
    RefreshFSMCommand cmdNightOn = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput outNightOn = refreshFSMTick(&cmdNightOn);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outNightOn.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outNightOn.framesToRelease.len);
    TEST_ASSERT_TRUE(outNightOn.isIdle);

    /* (2) REFRESH_CMD_NIGHT_MODE_OFF is sent, still before any frame or
       typical data has arrived. The FSM is not REFRESH_FSM_CLEARED, so
       this has no immediate effect beyond clearing the flag. */
    RefreshFSMCommand cmdNightOff = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput outNightOff = refreshFSMTick(&cmdNightOff);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outNightOff.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, outNightOff.framesToRelease.len);
    TEST_ASSERT_TRUE(outNightOff.isIdle);

    /* (3) the FSM bootstraps normally once frame and typical data arrive,
       exactly as if night mode had never been touched */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out1 = refreshFSMTick(&cmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out1.framesToRelease.len);
    TEST_ASSERT_TRUE(out1.isIdle);

    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput out2 = refreshFSMTick(&cmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out2.framesToRelease.len);
    TEST_ASSERT_TRUE(out2.isIdle);

    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand cmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput out3 = refreshFSMTick(&cmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, out3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, out3.framesToRelease.len);
    TEST_ASSERT_FALSE(out3.isIdle);

    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput out = refreshFSMTick(&cmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, out.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, out.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, out.framesToRelease.len);
        TEST_ASSERT_FALSE(out.isIdle);
    }

    RefreshFSMOutput outLast = refreshFSMTick(&cmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outLast.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, outLast.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, outLast.framesToRelease.len);
    TEST_ASSERT_TRUE(outLast.isIdle);
}

TEST_CASE("frameQueueCollapseWhileNightModeIsOn", TEST_GROUP)
{
    const uint32_t cmd1FrameNdx = 0;
    const uint32_t cmd2FrameNdx = 1;
    const uint32_t cmd3FrameNdx = 2;
    const uint32_t frameAFrameNdx = 3;
    const uint32_t frameBFrameNdx = 4;

    /* initialize variables */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledNumToReg[i].matrix = MAT1_PAGE0;
        ledNumToReg[i].red = 0x44;
        ledNumToReg[i].blue = 0x55;
        ledNumToReg[i].green = 0x66;
    }

    /* initialize FSM */
    RefreshFSMResources resources = {
        .LEDNumToReg = ledNumToReg,
        .LEDNumToRegLen = MAX_FRAME_SIZE,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_TEST_FRAMES,
        .slowLEDColor = { 0 },
        .mediumLEDColor = { 0 },
        .fastLEDColor = { 0 },
    };

    refreshFSMInit(&resources);

    /* send REFRESH_CMD_NEW_FRAME */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd1FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand fCmd1 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = cmd1FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput fOut1 = refreshFSMTick(&fCmd1);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, fOut1.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, fOut1.framesToRelease.len);
    TEST_ASSERT_TRUE(fOut1.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for NORTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd2FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand fCmd2 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd2FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = NORTH,
    };
    RefreshFSMOutput fOut2 = refreshFSMTick(&fCmd2);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, fOut2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, fOut2.framesToRelease.len);
    TEST_ASSERT_TRUE(fOut2.isIdle);

    /* send REFRESH_CMD_UPDATE_TYPICAL for SOUTH */
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        ledFrames[cmd3FrameNdx][i].ledNum = i;
    }
    RefreshFSMCommand fCmd3 = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = cmd3FrameNdx,
        .frameLen = MAX_FRAME_SIZE,
        .dir = SOUTH,
    };
    RefreshFSMOutput fOut3 = refreshFSMTick(&fCmd3);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, fOut3.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, fOut3.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, fOut3.framesToRelease.len);
    TEST_ASSERT_FALSE(fOut3.isIdle);

    /* send REFRESH_CMD_NONE until all LEDs of the frame are set */
    RefreshFSMCommand fCmd4 = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = 1; i < MAX_FRAME_SIZE - 1; i++)
    {
        RefreshFSMOutput fOut4 = refreshFSMTick(&fCmd4);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, fOut4.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, fOut4.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, fOut4.framesToRelease.len);
        TEST_ASSERT_FALSE(fOut4.isIdle);
    }

    RefreshFSMOutput fOut4Last = refreshFSMTick(&fCmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, fOut4Last.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, fOut4Last.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, fOut4Last.framesToRelease.len);
    TEST_ASSERT_TRUE(fOut4Last.isIdle);

    RefreshFSMOutput fOut5 = refreshFSMTick(&fCmd4);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, fOut5.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, fOut5.framesToRelease.len);
    TEST_ASSERT_TRUE(fOut5.isIdle);

    /* (2) NEW_FRAME for frame A, same direction (NORTH) as installed. The
       FSM queues frame A and begins clearing frame (1) from the board, one
       LED per tick, in reverse order. */
    for (uint32_t i = 0; i < 10; i++)
    {
        ledFrames[frameAFrameNdx][i].ledNum = 100 + i;
    }
    RefreshFSMCommand fCmd6 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameAFrameNdx,
        .frameLen = 10,
        .dir = NORTH,
    };
    RefreshFSMOutput fOut6 = refreshFSMTick(&fCmd6);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, fOut6.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE - 1, fOut6.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, fOut6.framesToRelease.len);
    TEST_ASSERT_FALSE(fOut6.isIdle);

    /* clear partway through the board, stopping well before frame (1) has
       finished clearing */
    RefreshFSMCommand fCmdNone = { .type = REFRESH_CMD_NONE };
    for (uint32_t i = MAX_FRAME_SIZE - 2; i >= 50; i--)
    {
        RefreshFSMOutput fOut7 = refreshFSMTick(&fCmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, fOut7.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, fOut7.action.clear.ledNum);
        TEST_ASSERT_EQUAL(0, fOut7.framesToRelease.len);
        TEST_ASSERT_FALSE(fOut7.isIdle);
    }

    /* (3) while REFRESH_ACTION_CLEAR is still being output, before frame
       (1) has finished clearing, REFRESH_CMD_NIGHT_MODE_ON is sent. This
       has no immediate effect beyond setting the night mode flag --
       clearing continues normally, uninterrupted, in the same tick. */
    RefreshFSMCommand fCmd8 = { .type = REFRESH_CMD_NIGHT_MODE_ON };
    RefreshFSMOutput fOut8 = refreshFSMTick(&fCmd8);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, fOut8.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(49, fOut8.action.clear.ledNum);
    TEST_ASSERT_EQUAL(0, fOut8.framesToRelease.len);
    TEST_ASSERT_FALSE(fOut8.isIdle);

    /* (4) while still outputting REFRESH_ACTION_CLEAR (night mode now on,
       frame (1) still not fully cleared), REFRESH_CMD_NEW_FRAME is sent
       again for frame B, same direction. Frame A is already queued, so
       the FSM immediately releases it as FRAME_RELEASE_QUEUED_STANDARD,
       abandons one-LED-at-a-time clearing, and outputs a single
       REFRESH_ACTION_CLEAR_RANGE -- but because night mode is on, the FSM
       goes idle with the board dark instead of installing frame B. Frame
       (1) is not released (only hidden, not replaced) and frame B is not
       latched in (it remains queued, as if queued while already dark). */
    for (uint32_t i = 0; i < 8; i++)
    {
        ledFrames[frameBFrameNdx][i].ledNum = 200 + i;
    }
    RefreshFSMCommand fCmd9 = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameBFrameNdx,
        .frameLen = 8,
        .dir = NORTH,
    };
    RefreshFSMOutput fOut9 = refreshFSMTick(&fCmd9);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR_RANGE, fOut9.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(49, fOut9.action.clearRange.startLedNum);
    TEST_ASSERT_EQUAL(1, fOut9.framesToRelease.len);
    TEST_ASSERT_EQUAL(frameAFrameNdx, fOut9.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_QUEUED_STANDARD, fOut9.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_TRUE(fOut9.isIdle);

    /* (5) the board stays dark and the FSM remains idle on further
       REFRESH_CMD_NONE ticks */
    RefreshFSMOutput fOut10 = refreshFSMTick(&fCmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, fOut10.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(0, fOut10.framesToRelease.len);
    TEST_ASSERT_TRUE(fOut10.isIdle);

    /* (6) REFRESH_CMD_NIGHT_MODE_OFF is sent. Frame B is queued, so the
       FSM releases frame (1) with a FRAME_RELEASE_STANDARD -- it is never
       redisplayed, only replaced by the frame that was queued before the
       board went dark -- and installs frame B instead. */
    RefreshFSMCommand fCmd11 = { .type = REFRESH_CMD_NIGHT_MODE_OFF };
    RefreshFSMOutput fOut11 = refreshFSMTick(&fCmd11);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, fOut11.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(200, fOut11.action.set.ledNum);
    TEST_ASSERT_EQUAL(1, fOut11.framesToRelease.len);
    TEST_ASSERT_EQUAL(cmd1FrameNdx, fOut11.framesToRelease.list[0].index);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_FSM_FRAME_RELEASE_STANDARD, fOut11.framesToRelease.list[0].type, "refresh frame release type");
    TEST_ASSERT_FALSE(fOut11.isIdle);

    /* the FSM installs frame B to completion with no further releases */
    for (uint32_t i = 201; i < 200 + 7; i++)
    {
        RefreshFSMOutput fOut = refreshFSMTick(&fCmdNone);
        TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, fOut.action.type, "refresh action type");
        TEST_ASSERT_EQUAL(i, fOut.action.set.ledNum);
        TEST_ASSERT_EQUAL(0, fOut.framesToRelease.len);
        TEST_ASSERT_FALSE(fOut.isIdle);
    }

    RefreshFSMOutput fOutLast2 = refreshFSMTick(&fCmdNone);
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, fOutLast2.action.type, "refresh action type");
    TEST_ASSERT_EQUAL(200 + 7, fOutLast2.action.set.ledNum);
    TEST_ASSERT_EQUAL(0, fOutLast2.framesToRelease.len);
    TEST_ASSERT_TRUE(fOutLast2.isIdle);
}
