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

#define NUM_TEST_FRAMES         (5)

TEST_CASE("defaultInitialization", TEST_GROUP)
{
    LEDReg ledNumToReg[MAX_FRAME_SIZE];
    LEDSpeed ledFrames[NUM_TEST_FRAMES][MAX_FRAME_SIZE];

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
    LEDReg ledNumToReg[MAX_FRAME_SIZE];
    LEDSpeed ledFrames[NUM_TEST_FRAMES][MAX_FRAME_SIZE];
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
    LEDReg ledNumToReg[MAX_FRAME_SIZE];
    LEDSpeed ledFrames[NUM_TEST_FRAMES][MAX_FRAME_SIZE];
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
    LEDReg ledNumToReg[MAX_FRAME_SIZE];
    LEDSpeed ledFrames[NUM_TEST_FRAMES][MAX_FRAME_SIZE];
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
    LEDReg ledNumToReg[MAX_FRAME_SIZE];
    LEDSpeed ledFrames[NUM_TEST_FRAMES][MAX_FRAME_SIZE];
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
    LEDReg ledNumToReg[MAX_FRAME_SIZE];
    LEDSpeed ledFrames[NUM_TEST_FRAMES][MAX_FRAME_SIZE];
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
    } else if (cmd3FrameNdx == out9.framesToRelease.list[0].index)
    {
        TEST_ASSERT_EQUAL(cmd1FrameNdx, out9.framesToRelease.list[1].index);
        TEST_ASSERT_EQUAL(REFRESH_FSM_FRAME_RELEASE_STANDARD, out9.framesToRelease.list[1].type);
        TEST_ASSERT_EQUAL(cmd3FrameNdx, out9.framesToRelease.list[0].index);
        TEST_ASSERT_EQUAL(REFRESH_FSM_FRAME_RELEASE_TYPICAL_SOUTH, out9.framesToRelease.list[0].type);
    } else
    {
        TEST_ASSERT_EQUAL(cmd1FrameNdx, out9.framesToRelease.list[0].index);
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