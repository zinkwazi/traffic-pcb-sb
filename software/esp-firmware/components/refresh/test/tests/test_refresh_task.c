/**
 * test_refresh_task.c
 *
 * Unit tests for refresh_task.h.
 *
 * Most tests here drive initRefreshTask()/handleRefreshTaskCommand()
 * directly instead of running the real refresh task, so they're
 * deterministic and don't depend on FreeRTOS scheduling. A small set of
 * "contract" tests still create and run the real task to verify the
 * public API (createRefreshTask/refreshLEDs/refreshEnableNightMode/
 * refreshDisableNightMode) is wired together correctly end to end.
 *
 * @note These tests call the real refresh_utilities and led_matrix APIs
 * against real hardware (see the test_refresh executable target); neither
 * is mocked, per the project's policy of faking only hardware dependencies
 * (and there is no led_matrix fake yet). initLedMatrix() tolerates being
 * called again across tests (ESP_ERR_INVALID_STATE is treated as success
 * by initRefreshTask()), since led_matrix has no deinit and stays
 * initialized for the rest of the test binary once the first test
 * initializes it.
 *
 * @note initRefreshTask()'s led_matrix-init-failure path can't be
 * exercised without fault injection, so it's deferred until a led_matrix
 * fake (CONFIG_FAKE_LED_MATRIX) exists.
 */

#include "refresh_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "unity.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "led_matrix.h"
#include "main_types.h"
#include "utilities.h"

#define TEST_GROUP "[refresh_task]"

/* An LED number guaranteed to be present on any populated V2.0/V2.1 board,
   matching the convention used by test_refresh_utilities.c. */
#define TEST_LED_NUM (1)

/* mirrors the private NUM_LED_FRAMES in refresh_task.c; command queue and
   frame pool are both sized to this */
#define TEST_NUM_LED_FRAMES (6)

#define REFRESH_TASK_NAME "RefreshTask"

/* Deletes the refresh task (if any) and resets task-global resources
   unconditionally, then reports whether deletion is confirmed. Must run
   before any TEST_ASSERT/TEST_FAIL in a test, since those abort the test
   function immediately and would otherwise skip cleanup, leaving the
   refresh task running into the next test. vTaskDelete() on a task other
   than the caller only unlinks it; freeing its TCB/stack is deferred to
   the idle task. None of these tests ever block, so idle never runs on
   its own - the vTaskDelay(1) below yields long enough for it to reclaim
   that memory before the next test creates another task, otherwise the
   15000-byte stacks pile up unreclaimed across the suite. */
static bool teardownRefreshTask(TaskHandle_t handle)
{
    if (NULL != handle)
    {
        vTaskDelete(handle);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    bool deleted = (NULL == xTaskGetHandle(REFRESH_TASK_NAME));
    refreshResetTaskResources();
    return deleted;
}

/* Creates task-global resources (commandQueue, the frame pool) without
   ever creating the refresh task itself, so initRefreshTask()/
   handleRefreshTaskCommand() can be called directly against real
   resources with no FreeRTOS task lifecycle involved at all. */
static esp_err_t setUpResourcesWithoutRunningTask(void)
{
    refreshResetTaskResources();
    return refreshCreateTaskResourcesForTest();
}

/* ---- contract-level tests: exercise the real public API and task ---- */

TEST_CASE("refreshLEDs_returnsInvalidArg_whenDataIsNull", TEST_GROUP)
{
    esp_err_t err = refreshLEDs(NULL, 1, NORTH, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshLEDs_returnsInvalidArg_whenDirIsNoDir", TEST_GROUP)
{
    LEDSpeed data[1] = { { .ledNum = 5, .speed = 10 } };
    esp_err_t err = refreshLEDs(data, 1, NO_DIR, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshLEDs_returnsInvalidArg_whenDataLenExceedsMax", TEST_GROUP)
{
    LEDSpeed data[1] = { { .ledNum = 5, .speed = 10 } };
    esp_err_t err = refreshLEDs(data, MAX_FRAME_SIZE + 1, NORTH, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/* refreshLEDs() only needs the frame pool / command queue to exist, not a
   running task consuming them (mirroring handleRefreshTaskCommand_
   releasesSupersededQueuedFrame_toPool below, which reserves frames the
   same way without ever creating the task). Driving these two boundary
   checks through setUpResourcesWithoutRunningTask() instead of a full
   createRefreshTask()/teardownRefreshTask() cycle avoids creating and
   immediately deleting a real FreeRTOS task purely to check an argument
   validation return code. */

TEST_CASE("refreshLEDs_acceptsZeroLengthFrame", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();

    LEDSpeed data[1] = { { .ledNum = 5, .speed = 10 } };
    /* dataLen == 0 is the lower boundary: no LEDs are copied, but a frame
       slot should still be reserved and queued successfully */
    esp_err_t ledsErr = refreshLEDs(data, 0, NORTH, 0);

    refreshResetTaskResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ledsErr, "refreshLEDs return code for zero-length frame");
}

TEST_CASE("refreshLEDs_acceptsFrameAtMaxSize", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();

    /* static: MAX_FRAME_SIZE entries is too large to comfortably put on
       the test task's stack */
    static LEDSpeed data[MAX_FRAME_SIZE];
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        data[i].ledNum = i;
        data[i].speed = 10;
    }

    /* dataLen == MAX_FRAME_SIZE is the upper boundary: contrast with
       refreshLEDs_returnsInvalidArg_whenDataLenExceedsMax, which checks
       MAX_FRAME_SIZE + 1 is rejected */
    esp_err_t ledsErr = refreshLEDs(data, MAX_FRAME_SIZE, NORTH, 0);

    refreshResetTaskResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ledsErr, "refreshLEDs return code for a frame at MAX_FRAME_SIZE");
}

TEST_CASE("createRefreshTask_returnsOk_onFirstCall", TEST_GROUP)
{
    refreshResetTaskResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_NOT_NULL_MESSAGE(handle, "task handle");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("createRefreshTask_returnsInvalidState_whenAlreadyCreated", TEST_GROUP)
{
    refreshResetTaskResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err1 = createRefreshTask(&handle, 1);
    esp_err_t err2 = createRefreshTask(NULL, 1);

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err1, "first createRefreshTask return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_STATE, err2, "second createRefreshTask return code");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshLEDs_returnsTimeout_whenFramesAndQueueAreFull", TEST_GROUP)
{
    refreshResetTaskResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    LEDSpeed data[1] = { { .ledNum = 5, .speed = 10 } };
    bool fillOk = true;
    if (ESP_OK == err)
    {
        for (int i = 0; i < TEST_NUM_LED_FRAMES; i++)
        {
            if (ESP_OK != refreshLEDs(data, 1, NORTH, 0))
            {
                fillOk = false;
                break;
            }
        }
    }

    esp_err_t overflowErr = ESP_FAIL;
    if (ESP_OK == err && fillOk)
    {
        overflowErr = refreshLEDs(data, 1, NORTH, 0);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_TRUE_MESSAGE(fillOk, "refreshLEDs failed while filling frames");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_TIMEOUT, overflowErr, "refreshLEDs return code once full");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshEnableNightMode_returnsTimeout_whenQueueIsFull", TEST_GROUP)
{
    refreshResetTaskResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    LEDSpeed data[1] = { { .ledNum = 5, .speed = 10 } };
    bool fillOk = true;
    if (ESP_OK == err)
    {
        for (int i = 0; i < TEST_NUM_LED_FRAMES; i++)
        {
            if (ESP_OK != refreshLEDs(data, 1, NORTH, 0))
            {
                fillOk = false;
                break;
            }
        }
    }

    esp_err_t overflowErr = ESP_FAIL;
    if (ESP_OK == err && fillOk)
    {
        overflowErr = refreshEnableNightMode(0);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_TRUE_MESSAGE(fillOk, "refreshLEDs failed while filling the queue");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_TIMEOUT, overflowErr, "refreshEnableNightMode return code once full");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshDisableNightMode_returnsTimeout_whenQueueIsFull", TEST_GROUP)
{
    refreshResetTaskResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    LEDSpeed data[1] = { { .ledNum = 5, .speed = 10 } };
    bool fillOk = true;
    if (ESP_OK == err)
    {
        for (int i = 0; i < TEST_NUM_LED_FRAMES; i++)
        {
            if (ESP_OK != refreshLEDs(data, 1, NORTH, 0))
            {
                fillOk = false;
                break;
            }
        }
    }

    esp_err_t overflowErr = ESP_FAIL;
    if (ESP_OK == err && fillOk)
    {
        overflowErr = refreshDisableNightMode(0);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_TRUE_MESSAGE(fillOk, "refreshLEDs failed while filling the queue");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_TIMEOUT, overflowErr, "refreshDisableNightMode return code once full");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshEnableNightMode_returnsOk_whenQueueHasSpace", TEST_GROUP)
{
    refreshResetTaskResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    esp_err_t nightModeErr = ESP_FAIL;
    if (ESP_OK == err)
    {
        nightModeErr = refreshEnableNightMode(0);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, nightModeErr, "refreshEnableNightMode return code");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshDisableNightMode_returnsOk_whenQueueHasSpace", TEST_GROUP)
{
    refreshResetTaskResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    esp_err_t nightModeErr = ESP_FAIL;
    if (ESP_OK == err)
    {
        nightModeErr = refreshDisableNightMode(0);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, nightModeErr, "refreshDisableNightMode return code");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

/* ---- initRefreshTask() unit tests: no task is ever run ---- */

TEST_CASE("refreshCreateTaskResourcesForTest_returnsInvalidState_whenAlreadyCreated", TEST_GROUP)
{
    refreshResetTaskResources();

    esp_err_t err1 = refreshCreateTaskResourcesForTest();
    esp_err_t err2 = refreshCreateTaskResourcesForTest();

    refreshResetTaskResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err1, "first refreshCreateTaskResourcesForTest return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_STATE, err2, "second refreshCreateTaskResourcesForTest return code");
}

TEST_CASE("initRefreshTask_returnsFalse_whenResourcesNotCreated", TEST_GROUP)
{
    refreshResetTaskResources();

    bool ok = initRefreshTask();

    TEST_ASSERT_FALSE(ok);
}

TEST_CASE("initRefreshTask_initializesFSM_whenResourcesExist", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    bool ok = initRefreshTask();

    /* the FSM should now be freshly initialized and idle, waiting for its
       first frame; REFRESH_CMD_NONE should provoke no action at all */
    RefreshFSMCommand cmd = { .type = REFRESH_CMD_NONE, .frameNdx = UINT32_MAX, .frameLen = UINT32_MAX, .dir = NO_DIR };
    RefreshFSMOutput out;
    handleRefreshTaskCommand(&cmd, &out);

    refreshResetTaskResources();

    TEST_ASSERT_TRUE_MESSAGE(ok, "initRefreshTask return value");
    TEST_ASSERT_TRUE_MESSAGE(out.isIdle, "FSM should be idle waiting for its first frame after init");
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out.action.type, "no LED action should be taken for REFRESH_CMD_NONE");
}

/* ---- handleRefreshTaskCommand() unit tests: frame pool ("queue")
   behavior, driven directly with no task and no command queue involved ---- */

TEST_CASE("handleRefreshTaskCommand_releasesNothing_forFirstFrame", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    bool initOk = initRefreshTask();

    uint32_t availableBefore = refreshGetAvailableFrameCount();

    RefreshFSMCommand cmd = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = 0, .frameLen = 0, .dir = NORTH };
    RefreshFSMOutput out;
    handleRefreshTaskCommand(&cmd, &out);

    uint32_t availableAfter = refreshGetAvailableFrameCount();

    refreshResetTaskResources();

    TEST_ASSERT_TRUE_MESSAGE(initOk, "initRefreshTask return value");
    TEST_ASSERT_EQUAL_MESSAGE(0, out.framesToRelease.len, "no frame should be released for the first frame received");
    TEST_ASSERT_EQUAL_MESSAGE(availableBefore, availableAfter, "frame pool should be untouched");
}

TEST_CASE("handleRefreshTaskCommand_releasesSupersededQueuedFrame_toPool", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    bool initOk = initRefreshTask();

    /* reserve 3 real frame slots (guaranteed to be 0, 1, 2 in order, since
       the pool is a FIFO queue and starts as [0..NUM_LED_FRAMES)), so
       releasing one of them back is a legal push and can't overflow the
       pool */
    LEDSpeed data[1] = { { .ledNum = 5, .speed = 10 } };
    esp_err_t reserve0 = refreshLEDs(data, 1, NORTH, 0);
    esp_err_t reserve1 = refreshLEDs(data, 1, NORTH, 0);
    esp_err_t reserve2 = refreshLEDs(data, 1, NORTH, 0);

    uint32_t availableBefore = refreshGetAvailableFrameCount();

    /* while still waiting for typical frames, the FSM only latches the
       first frame; a second buffers as "next"; a third supersedes and
       releases that buffered "next" frame (frame 1) back to the pool */
    RefreshFSMCommand cmd1 = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = 0, .frameLen = 0, .dir = NORTH };
    RefreshFSMCommand cmd2 = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = 1, .frameLen = 0, .dir = NORTH };
    RefreshFSMCommand cmd3 = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = 2, .frameLen = 0, .dir = NORTH };
    RefreshFSMOutput out1, out2, out3;
    handleRefreshTaskCommand(&cmd1, &out1);
    handleRefreshTaskCommand(&cmd2, &out2);
    handleRefreshTaskCommand(&cmd3, &out3);

    uint32_t availableAfter = refreshGetAvailableFrameCount();

    refreshResetTaskResources();

    TEST_ASSERT_TRUE_MESSAGE(initOk, "initRefreshTask return value");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserve0, "reserving frame 0");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserve1, "reserving frame 1");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserve2, "reserving frame 2");
    TEST_ASSERT_EQUAL_MESSAGE(0, out1.framesToRelease.len, "first frame should not release anything");
    TEST_ASSERT_EQUAL_MESSAGE(0, out2.framesToRelease.len, "second frame should only buffer, not release");
    TEST_ASSERT_EQUAL_MESSAGE(1, out3.framesToRelease.len, "third frame should release the superseded second frame");
    TEST_ASSERT_EQUAL_MESSAGE(1, out3.framesToRelease.list[0].index, "released frame should be the superseded index");
    TEST_ASSERT_EQUAL_MESSAGE(availableBefore + 1, availableAfter, "released frame should be returned to the pool");
}

/* ---- end-to-end hardware tests: verify handleRefreshTaskCommand()'s
   dispatch of FSM actions actually reaches real led_matrix hardware, not
   just that the right RefreshFSMAction is returned. Frame slots are
   reserved and populated via refreshLEDs() (as in the tests above), then
   fed back into handleRefreshTaskCommand() directly with hand-built
   commands so the FSM state machine can be driven deterministically
   through WAITING_FOR_FRAMES -> INSTALLING_FRAME -> CLEARING_FRAME
   without a running task or command queue involved. ---- */

TEST_CASE("handleRefreshTaskCommand_installThenNightMode_writesRealColorsToLedMatrix", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    /* initRefreshTask() calls matReset(), so the board is known-blank
       going into this test */
    bool initOk = initRefreshTask();

    /* a single live LED, well below its typical speed so it unambiguously
       classifies as "slow" (< 50% of typical) regardless of where the
       exact cutoff boundary falls */
    LEDSpeed currData[1] = { { .ledNum = TEST_LED_NUM, .speed = 20 } };
    esp_err_t reserveCurr = refreshLEDs(currData, 1, NORTH, 0);

    /* typical frames must be exactly MAX_FRAME_SIZE long and indexed by
       LED number (ledFrames[ndx][ledNum].ledNum == ledNum), matching the
       lookup the FSM performs in ledSpeedToColor() */
    static LEDSpeed typicalNorth[MAX_FRAME_SIZE];
    static LEDSpeed typicalSouth[MAX_FRAME_SIZE];
    for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
    {
        typicalNorth[i].ledNum = i;
        typicalNorth[i].speed = 100;
        typicalSouth[i].ledNum = i;
        typicalSouth[i].speed = 100;
    }
    esp_err_t reserveTypicalNorth = refreshLEDs(typicalNorth, MAX_FRAME_SIZE, NORTH, 0);
    esp_err_t reserveTypicalSouth = refreshLEDs(typicalSouth, MAX_FRAME_SIZE, SOUTH, 0);

    /* while only the current frame is known, the FSM stays in
       WAITING_FOR_FRAMES and takes no action */
    RefreshFSMCommand cmdCurr = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = 0, .frameLen = 1, .dir = NORTH };
    RefreshFSMOutput outCurr;
    handleRefreshTaskCommand(&cmdCurr, &outCurr);

    /* the north typical frame alone still isn't enough; south is still missing */
    RefreshFSMCommand cmdTypicalNorth = { .type = REFRESH_CMD_UPDATE_TYPICAL, .frameNdx = 1, .frameLen = MAX_FRAME_SIZE, .dir = NORTH };
    RefreshFSMOutput outTypicalNorth;
    handleRefreshTaskCommand(&cmdTypicalNorth, &outTypicalNorth);

    /* south typical frame is the last piece: the FSM should immediately
       install the single-LED current frame, which means
       handleRefreshTaskCommand() has already dispatched a real
       REFRESH_ACTION_SET to led_matrix by the time this call returns */
    RefreshFSMCommand cmdTypicalSouth = { .type = REFRESH_CMD_UPDATE_TYPICAL, .frameNdx = 2, .frameLen = MAX_FRAME_SIZE, .dir = SOUTH };
    RefreshFSMOutput outTypicalSouth;
    handleRefreshTaskCommand(&cmdTypicalSouth, &outTypicalSouth);

    uint8_t installedRed = 0, installedGreen = 0, installedBlue = 0;
    esp_err_t readInstalled = matGetColor(TEST_LED_NUM, &installedRed, &installedGreen, &installedBlue);

    /* turning night mode on while a frame is installed should clear the
       single lit LED in this same tick, dispatching a real
       REFRESH_ACTION_CLEAR to led_matrix */
    RefreshFSMCommand cmdNightModeOn = { .type = REFRESH_CMD_NIGHT_MODE_ON, .frameNdx = 0, .frameLen = 0, .dir = NO_DIR };
    RefreshFSMOutput outNightModeOn;
    handleRefreshTaskCommand(&cmdNightModeOn, &outNightModeOn);

    uint8_t clearedRed = 0, clearedGreen = 0, clearedBlue = 0;
    esp_err_t readCleared = matGetColor(TEST_LED_NUM, &clearedRed, &clearedGreen, &clearedBlue);

    refreshResetTaskResources();

    TEST_ASSERT_TRUE_MESSAGE(initOk, "initRefreshTask return value");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserveCurr, "reserving current frame");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserveTypicalNorth, "reserving north typical frame");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserveTypicalSouth, "reserving south typical frame");

    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outCurr.action.type, "no action while only the current frame is known");
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, outTypicalNorth.action.type, "no action while south typical frame is still missing");

    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outTypicalSouth.action.type, "FSM action once both typical frames arrive");
    TEST_ASSERT_EQUAL_MESSAGE(TEST_LED_NUM, outTypicalSouth.action.set.ledNum, "FSM should set the only LED in the current frame");
    TEST_ASSERT_TRUE_MESSAGE(outTypicalSouth.isIdle, "FSM should be idle once the single-LED frame is fully installed");

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, readInstalled, "matGetColor after install");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(SLOW_RED, installedRed, "installed LED red channel should match the slow color");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(SLOW_GREEN, installedGreen, "installed LED green channel should match the slow color");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(SLOW_BLUE, installedBlue, "installed LED blue channel should match the slow color");

    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, outNightModeOn.action.type, "FSM action when night mode turns on with a frame installed");
    TEST_ASSERT_EQUAL_MESSAGE(TEST_LED_NUM, outNightModeOn.action.clear.ledNum, "FSM should clear the only LED in the current frame");
    TEST_ASSERT_TRUE_MESSAGE(outNightModeOn.isIdle, "FSM should be idle once the single-LED frame is fully cleared");

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, readCleared, "matGetColor after night mode clear");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, clearedRed, "cleared LED red channel");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, clearedGreen, "cleared LED green channel");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, clearedBlue, "cleared LED blue channel");
}
