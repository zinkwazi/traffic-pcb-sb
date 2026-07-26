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

#define NUM_TEST_FRAMES         (6)

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

TEST_CASE("refreshCommandDuringClearing", TEST_GROUP)
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

TEST_CASE("refreshCommandDuringInstallation", TEST_GROUP)
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
    LEDReg ledNumToReg[MAX_FRAME_SIZE];
    LEDSpeed ledFrames[NUM_TEST_FRAMES][MAX_FRAME_SIZE];
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
