/**
 * test_refresh_task.c
 *
 * Unit tests for refresh_task.h.
 *
 * Most tests here drive refreshTaskInit()/refreshTaskHandleCommand()
 * directly instead of running the real refresh task, so they're
 * deterministic and don't depend on FreeRTOS scheduling. A small set of
 * "contract" tests still create and run the real task to verify the
 * public API (createRefreshTask/refreshTaskRefreshLEDs/refreshTaskEnableNightMode/
 * refreshTaskDisableNightMode) is wired together correctly end to end.
 *
 * @note These tests call the real refresh_utilities and led_matrix APIs
 * against real hardware (see the test_refresh executable target); neither
 * is mocked, per the project's policy of faking only hardware dependencies
 * (and there is no led_matrix fake yet). initLedMatrix() tolerates being
 * called again across tests (ESP_ERR_INVALID_STATE is treated as success
 * by refreshTaskInit()), since led_matrix has no deinit and stays
 * initialized for the rest of the test binary once the first test
 * initializes it.
 *
 * @note refreshTaskInit()'s led_matrix-init-failure path can't be
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
#include "led_registers.h"
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
    refreshTaskResetResources();
    return deleted;
}

/* Creates task-global resources (commandQueue, the frame pool) without
   ever creating the refresh task itself, so refreshTaskInit()/
   refreshTaskHandleCommand() can be called directly against real
   resources with no FreeRTOS task lifecycle involved at all. */
static esp_err_t setUpResourcesWithoutRunningTask(void)
{
    refreshTaskResetResources();
    return refreshTaskCreateResourcesForTest();
}

/* Fills commandQueue with night mode commands, which need no frame slot,
   so the frame pool is left untouched. Returns true if every send
   succeeded. Only fills the queue if nothing is consuming it. */
static bool fillCommandQueue(void)
{
    for (int i = 0; i < TEST_NUM_LED_FRAMES; i++)
    {
        if (ESP_OK != refreshTaskDisableNightMode(NULL, 0)) return false;
    }
    return true;
}

/* ---- contract-level tests: exercise the real public API and task ---- */

/* The argument checks below happen before any task resource is touched,
   so they need no setup. Tests that reserve frames or send commands only
   need the frame pool / command queue to exist, not a running task
   consuming them, so they use setUpResourcesWithoutRunningTask() instead
   of a full createRefreshTask()/teardownRefreshTask() cycle. */

TEST_CASE("refreshTaskReserveFrame_returnsInvalidArg_whenFrameIsNull", TEST_GROUP)
{
    uint32_t frameNdx;
    esp_err_t err = refreshTaskReserveFrame(NULL, &frameNdx, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshTaskReserveFrame_returnsInvalidArg_whenFrameNdxIsNull", TEST_GROUP)
{
    LEDSpeed (*frame)[MAX_FRAME_SIZE];
    esp_err_t err = refreshTaskReserveFrame(&frame, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshTaskReserveFrame_returnsDistinctFrames_untilPoolIsEmpty", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();

    LEDSpeed (*frames[TEST_NUM_LED_FRAMES])[MAX_FRAME_SIZE];
    uint32_t frameNdxs[TEST_NUM_LED_FRAMES];
    esp_err_t reserveErrs[TEST_NUM_LED_FRAMES];
    bool reserved[TEST_NUM_LED_FRAMES];
    for (int i = 0; i < TEST_NUM_LED_FRAMES; i++)
    {
        reserveErrs[i] = refreshTaskReserveFrame(&frames[i], &frameNdxs[i], 0);
        reserved[i] = refreshTaskIsFrameReserved(frameNdxs[i]);
    }

    LEDSpeed (*overflowFrame)[MAX_FRAME_SIZE];
    uint32_t overflowNdx;
    esp_err_t overflowErr = refreshTaskReserveFrame(&overflowFrame, &overflowNdx, 0);

    refreshTaskResetResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");
    for (int i = 0; i < TEST_NUM_LED_FRAMES; i++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserveErrs[i], "refreshTaskReserveFrame return code while pool has frames");
        TEST_ASSERT_LESS_THAN_MESSAGE(TEST_NUM_LED_FRAMES, frameNdxs[i], "reserved frame index in range");
        TEST_ASSERT_TRUE_MESSAGE(reserved[i], "frame should be marked reserved");
        for (int j = 0; j < i; j++)
        {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(frameNdxs[j], frameNdxs[i], "reserved frame indices should be distinct");
            TEST_ASSERT_TRUE_MESSAGE(frames[j] != frames[i], "reserved frames should be distinct");
        }
    }
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_TIMEOUT, overflowErr, "refreshTaskReserveFrame return code once pool is empty");
}

TEST_CASE("refreshTaskReleaseFrame_returnsInvalidArg_whenFrameNdxOutOfRange", TEST_GROUP)
{
    esp_err_t err = refreshTaskReleaseFrame(TEST_NUM_LED_FRAMES, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshTaskReleaseFrame_returnsInvalidArg_whenFrameNotReserved", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();

    esp_err_t releaseErr = refreshTaskReleaseFrame(0, 0);

    refreshTaskResetResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_ARG, releaseErr, "refreshTaskReleaseFrame return code for an unreserved frame");
}

TEST_CASE("refreshTaskReleaseFrame_returnsFrameToPool_whenReserved", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();

    LEDSpeed (*frame)[MAX_FRAME_SIZE];
    uint32_t frameNdx = UINT32_MAX;
    esp_err_t reserveErr = refreshTaskReserveFrame(&frame, &frameNdx, 0);
    uint32_t availableBefore = refreshTaskGetAvailableFrameCount();

    esp_err_t releaseErr = refreshTaskReleaseFrame(frameNdx, 0);
    uint32_t availableAfter = refreshTaskGetAvailableFrameCount();
    bool stillReserved = refreshTaskIsFrameReserved(frameNdx);

    /* a second release of the same frame must be rejected */
    esp_err_t doubleReleaseErr = refreshTaskReleaseFrame(frameNdx, 0);

    refreshTaskResetResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserveErr, "refreshTaskReserveFrame return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, releaseErr, "refreshTaskReleaseFrame return code");
    TEST_ASSERT_EQUAL_MESSAGE(availableBefore + 1, availableAfter, "released frame should be returned to the pool");
    TEST_ASSERT_FALSE_MESSAGE(stillReserved, "released frame should no longer be marked reserved");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_ARG, doubleReleaseErr, "second refreshTaskReleaseFrame return code");
}

TEST_CASE("refreshTaskRefreshLEDs_returnsInvalidArg_whenFrameNdxOutOfRange", TEST_GROUP)
{
    esp_err_t err = refreshTaskRefreshLEDs(TEST_NUM_LED_FRAMES, 1, NORTH, NULL, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshTaskRefreshLEDs_returnsInvalidArg_whenDirIsNoDir", TEST_GROUP)
{
    esp_err_t err = refreshTaskRefreshLEDs(0, 1, NO_DIR, NULL, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshTaskRefreshLEDs_returnsInvalidArg_whenFrameLenExceedsMax", TEST_GROUP)
{
    esp_err_t err = refreshTaskRefreshLEDs(0, MAX_FRAME_SIZE + 1, NORTH, NULL, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshTaskRefreshLEDs_acceptsZeroLengthFrame", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();

    LEDSpeed (*frame)[MAX_FRAME_SIZE];
    uint32_t frameNdx = UINT32_MAX;
    esp_err_t reserveErr = refreshTaskReserveFrame(&frame, &frameNdx, 0);

    /* frameLen == 0 is the lower boundary: the frame holds no LEDs, but
       the command should still be queued successfully */
    esp_err_t ledsErr = refreshTaskRefreshLEDs(frameNdx, 0, NORTH, NULL, NULL, 0);

    refreshTaskResetResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserveErr, "refreshTaskReserveFrame return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ledsErr, "refreshTaskRefreshLEDs return code for zero-length frame");
}

TEST_CASE("refreshTaskRefreshLEDs_acceptsFrameAtMaxSize", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();

    LEDSpeed (*frame)[MAX_FRAME_SIZE];
    uint32_t frameNdx = UINT32_MAX;
    esp_err_t reserveErr = refreshTaskReserveFrame(&frame, &frameNdx, 0);
    if (ESP_OK == reserveErr)
    {
        for (uint32_t i = 0; i < MAX_FRAME_SIZE; i++)
        {
            (*frame)[i].ledNum = i;
            (*frame)[i].speed = 10;
        }
    }

    /* frameLen == MAX_FRAME_SIZE is the upper boundary: contrast with
       refreshTaskRefreshLEDs_returnsInvalidArg_whenFrameLenExceedsMax, which checks
       MAX_FRAME_SIZE + 1 is rejected */
    esp_err_t ledsErr = refreshTaskRefreshLEDs(frameNdx, MAX_FRAME_SIZE, NORTH, NULL, NULL, 0);

    refreshTaskResetResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserveErr, "refreshTaskReserveFrame return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ledsErr, "refreshTaskRefreshLEDs return code for a frame at MAX_FRAME_SIZE");
}

TEST_CASE("refreshTaskUpdateTypical_returnsInvalidArg_whenFrameNdxOutOfRange", TEST_GROUP)
{
    esp_err_t err = refreshTaskUpdateTypical(TEST_NUM_LED_FRAMES, MAX_NUM_LEDS_REG + 1, NORTH, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshTaskUpdateTypical_returnsInvalidArg_whenDirIsNoDir", TEST_GROUP)
{
    esp_err_t err = refreshTaskUpdateTypical(0, MAX_NUM_LEDS_REG + 1, NO_DIR, NULL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("refreshTaskUpdateTypical_returnsInvalidArg_whenFrameLenIsNotLEDCountPlusOne", TEST_GROUP)
{
    esp_err_t errShort = refreshTaskUpdateTypical(0, MAX_NUM_LEDS_REG, NORTH, NULL, 0);
    esp_err_t errLong = refreshTaskUpdateTypical(0, MAX_NUM_LEDS_REG + 2, NORTH, NULL, 0);
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_ARG, errShort, "frameLen one short");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_ARG, errLong, "frameLen one long");
}

TEST_CASE("refreshTaskUpdateTypical_acceptsFullTypicalFrame", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();

    LEDSpeed (*frame)[MAX_FRAME_SIZE];
    uint32_t frameNdx = UINT32_MAX;
    esp_err_t reserveErr = refreshTaskReserveFrame(&frame, &frameNdx, 0);
    if (ESP_OK == reserveErr)
    {
        for (uint32_t i = 0; i <= MAX_NUM_LEDS_REG; i++)
        {
            (*frame)[i].ledNum = i;
            (*frame)[i].speed = 100;
        }
    }

    esp_err_t typicalErr = refreshTaskUpdateTypical(frameNdx, MAX_NUM_LEDS_REG + 1, NORTH, NULL, 0);

    refreshTaskResetResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserveErr, "refreshTaskReserveFrame return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, typicalErr, "refreshTaskUpdateTypical return code");
}

TEST_CASE("createRefreshTask_returnsOk_onFirstCall", TEST_GROUP)
{
    refreshTaskResetResources();
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
    refreshTaskResetResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err1 = createRefreshTask(&handle, 1);
    esp_err_t err2 = createRefreshTask(NULL, 1);

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err1, "first createRefreshTask return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_STATE, err2, "second createRefreshTask return code");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshTaskRefreshLEDs_returnsTimeout_whenCommandQueueIsFull", TEST_GROUP)
{
    refreshTaskResetResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    bool fillOk = false;
    esp_err_t reserveErr = ESP_FAIL;
    LEDSpeed (*frame)[MAX_FRAME_SIZE];
    uint32_t frameNdx = UINT32_MAX;
    if (ESP_OK == err)
    {
        fillOk = fillCommandQueue();
        reserveErr = refreshTaskReserveFrame(&frame, &frameNdx, 0);
    }

    esp_err_t overflowErr = ESP_FAIL;
    bool stillReserved = false;
    if (ESP_OK == err && fillOk && ESP_OK == reserveErr)
    {
        (*frame)[0].ledNum = 5;
        (*frame)[0].speed = 10;
        overflowErr = refreshTaskRefreshLEDs(frameNdx, 1, NORTH, NULL, NULL, 0);
        /* on failure the caller keeps ownership of the frame */
        stillReserved = refreshTaskIsFrameReserved(frameNdx);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_TRUE_MESSAGE(fillOk, "failed while filling the command queue");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserveErr, "refreshTaskReserveFrame return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_TIMEOUT, overflowErr, "refreshTaskRefreshLEDs return code once full");
    TEST_ASSERT_TRUE_MESSAGE(stillReserved, "frame should still be reserved by the caller after a failed send");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshTaskEnableNightMode_returnsTimeout_whenQueueIsFull", TEST_GROUP)
{
    refreshTaskResetResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    bool fillOk = false;
    if (ESP_OK == err)
    {
        fillOk = fillCommandQueue();
    }

    esp_err_t overflowErr = ESP_FAIL;
    if (ESP_OK == err && fillOk)
    {
        overflowErr = refreshTaskEnableNightMode(NULL, 0);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_TRUE_MESSAGE(fillOk, "failed while filling the command queue");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_TIMEOUT, overflowErr, "refreshTaskEnableNightMode return code once full");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshTaskDisableNightMode_returnsTimeout_whenQueueIsFull", TEST_GROUP)
{
    refreshTaskResetResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    bool fillOk = false;
    if (ESP_OK == err)
    {
        fillOk = fillCommandQueue();
    }

    esp_err_t overflowErr = ESP_FAIL;
    if (ESP_OK == err && fillOk)
    {
        overflowErr = refreshTaskDisableNightMode(NULL, 0);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_TRUE_MESSAGE(fillOk, "failed while filling the command queue");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_TIMEOUT, overflowErr, "refreshTaskDisableNightMode return code once full");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshTaskEnableNightMode_returnsOk_whenQueueHasSpace", TEST_GROUP)
{
    refreshTaskResetResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    esp_err_t nightModeErr = ESP_FAIL;
    if (ESP_OK == err)
    {
        nightModeErr = refreshTaskEnableNightMode(NULL, 0);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, nightModeErr, "refreshTaskEnableNightMode return code");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

TEST_CASE("refreshTaskDisableNightMode_returnsOk_whenQueueHasSpace", TEST_GROUP)
{
    refreshTaskResetResources();
    vTaskPrioritySet(NULL, 2);

    TaskHandle_t handle = NULL;
    esp_err_t err = createRefreshTask(&handle, 1);

    esp_err_t nightModeErr = ESP_FAIL;
    if (ESP_OK == err)
    {
        nightModeErr = refreshTaskDisableNightMode(NULL, 0);
    }

    bool deleted = teardownRefreshTask(handle);

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "createRefreshTask return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, nightModeErr, "refreshTaskDisableNightMode return code");
    TEST_ASSERT_TRUE_MESSAGE(deleted, "refresh task still resolvable after deletion");
}

/* ---- refreshTaskInit() unit tests: no task is ever run ---- */

TEST_CASE("refreshTaskCreateResourcesForTest_returnsInvalidState_whenAlreadyCreated", TEST_GROUP)
{
    refreshTaskResetResources();

    esp_err_t err1 = refreshTaskCreateResourcesForTest();
    esp_err_t err2 = refreshTaskCreateResourcesForTest();

    refreshTaskResetResources();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err1, "first refreshTaskCreateResourcesForTest return code");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_STATE, err2, "second refreshTaskCreateResourcesForTest return code");
}

TEST_CASE("refreshTaskInit_returnsFalse_whenResourcesNotCreated", TEST_GROUP)
{
    refreshTaskResetResources();

    bool ok = refreshTaskInit();

    TEST_ASSERT_FALSE(ok);
}

TEST_CASE("refreshTaskInit_initializesFSM_whenResourcesExist", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    bool ok = refreshTaskInit();

    /* the FSM should now be freshly initialized and idle, waiting for its
       first frame; REFRESH_CMD_NONE should provoke no action at all */
    RefreshFSMCommand cmd = { .type = REFRESH_CMD_NONE, .frameNdx = UINT32_MAX, .frameLen = UINT32_MAX, .dir = NO_DIR };
    RefreshFSMOutput out;
    refreshTaskHandleCommand(&cmd, &out);

    refreshTaskResetResources();

    TEST_ASSERT_TRUE_MESSAGE(ok, "refreshTaskInit return value");
    TEST_ASSERT_TRUE_MESSAGE(out.isIdle, "FSM should be idle waiting for its first frame after init");
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_NONE, out.action.type, "no LED action should be taken for REFRESH_CMD_NONE");
}

/* ---- refreshTaskHandleCommand() unit tests: frame pool ("queue")
   behavior, driven directly with no task and no command queue involved ---- */

TEST_CASE("refreshTaskHandleCommand_releasesNothing_forFirstFrame", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    bool initOk = refreshTaskInit();

    uint32_t availableBefore = refreshTaskGetAvailableFrameCount();

    RefreshFSMCommand cmd = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = 0, .frameLen = 0, .dir = NORTH };
    RefreshFSMOutput out;
    refreshTaskHandleCommand(&cmd, &out);

    uint32_t availableAfter = refreshTaskGetAvailableFrameCount();

    refreshTaskResetResources();

    TEST_ASSERT_TRUE_MESSAGE(initOk, "refreshTaskInit return value");
    TEST_ASSERT_EQUAL_MESSAGE(0, out.framesToRelease.len, "no frame should be released for the first frame received");
    TEST_ASSERT_EQUAL_MESSAGE(availableBefore, availableAfter, "frame pool should be untouched");
}

TEST_CASE("refreshTaskHandleCommand_setsReceived_onceDisplacedFrameIsReleased", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    bool initOk = refreshTaskInit();

    LEDSpeed (*frame0)[MAX_FRAME_SIZE], (*frame1)[MAX_FRAME_SIZE], (*frame2)[MAX_FRAME_SIZE];
    uint32_t ndx0 = UINT32_MAX, ndx1 = UINT32_MAX, ndx2 = UINT32_MAX;
    esp_err_t reserve0 = refreshTaskReserveFrame(&frame0, &ndx0, 0);
    esp_err_t reserve1 = refreshTaskReserveFrame(&frame1, &ndx1, 0);
    esp_err_t reserve2 = refreshTaskReserveFrame(&frame2, &ndx2, 0);

    /* the second frame is only buffered behind the first, and the third
       displaces it; each is received as soon as it is handled */
    volatile bool received1 = false, received2 = false, received3 = false, receivedNightOn = false;
    RefreshFSMCommand cmd1 = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = ndx0, .frameLen = 0, .dir = NORTH, .received = &received1 };
    RefreshFSMCommand cmd2 = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = ndx1, .frameLen = 0, .dir = NORTH, .received = &received2 };
    RefreshFSMCommand cmd3 = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = ndx2, .frameLen = 0, .dir = NORTH, .received = &received3 };
    RefreshFSMCommand cmdNightOn = { .type = REFRESH_CMD_NIGHT_MODE_ON, .received = &receivedNightOn };
    RefreshFSMOutput out1, out2, out3, outNightOn;

    refreshTaskHandleCommand(&cmd1, &out1);
    bool received1AfterCmd1 = received1;
    refreshTaskHandleCommand(&cmd2, &out2);
    bool received2AfterCmd2 = received2;
    refreshTaskHandleCommand(&cmd3, &out3);
    bool received3AfterCmd3 = received3;
    bool displacedStillReserved = refreshTaskIsFrameReserved(ndx1);
    refreshTaskHandleCommand(&cmdNightOn, &outNightOn);
    bool receivedNightOnAfter = receivedNightOn;

    refreshTaskResetResources();

    TEST_ASSERT_TRUE_MESSAGE(initOk, "refreshTaskInit return value");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserve0, "reserving frame 0");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserve1, "reserving frame 1");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserve2, "reserving frame 2");
    TEST_ASSERT_TRUE_MESSAGE(received1AfterCmd1, "first frame received when handled");
    TEST_ASSERT_TRUE_MESSAGE(received2AfterCmd2, "buffered frame received when handled, not when latched");
    TEST_ASSERT_EQUAL_MESSAGE(1, out3.framesToRelease.len, "third frame should displace the second");
    TEST_ASSERT_TRUE_MESSAGE(received3AfterCmd3, "displacing frame received when handled");
    TEST_ASSERT_FALSE_MESSAGE(displacedStillReserved, "displaced frame is back in the pool by the time received is set");
    TEST_ASSERT_TRUE_MESSAGE(receivedNightOnAfter, "frameless command received when handled");
}

TEST_CASE("refreshTaskHandleCommand_releasesSupersededQueuedFrame_toPool", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    bool initOk = refreshTaskInit();

    /* reserve 3 real frame slots, so releasing one of them back is a
       legal push and can't overflow the pool */
    LEDSpeed (*frame0)[MAX_FRAME_SIZE], (*frame1)[MAX_FRAME_SIZE], (*frame2)[MAX_FRAME_SIZE];
    uint32_t ndx0 = UINT32_MAX, ndx1 = UINT32_MAX, ndx2 = UINT32_MAX;
    esp_err_t reserve0 = refreshTaskReserveFrame(&frame0, &ndx0, 0);
    esp_err_t reserve1 = refreshTaskReserveFrame(&frame1, &ndx1, 0);
    esp_err_t reserve2 = refreshTaskReserveFrame(&frame2, &ndx2, 0);

    uint32_t availableBefore = refreshTaskGetAvailableFrameCount();

    /* while still waiting for typical frames, the FSM only latches the
       first frame; a second buffers as "next"; a third supersedes and
       releases that buffered "next" frame (ndx1) back to the pool */
    RefreshFSMCommand cmd1 = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = ndx0, .frameLen = 0, .dir = NORTH };
    RefreshFSMCommand cmd2 = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = ndx1, .frameLen = 0, .dir = NORTH };
    RefreshFSMCommand cmd3 = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = ndx2, .frameLen = 0, .dir = NORTH };
    RefreshFSMOutput out1, out2, out3;
    refreshTaskHandleCommand(&cmd1, &out1);
    refreshTaskHandleCommand(&cmd2, &out2);
    refreshTaskHandleCommand(&cmd3, &out3);

    uint32_t availableAfter = refreshTaskGetAvailableFrameCount();
    bool releasedStillReserved = refreshTaskIsFrameReserved(ndx1);

    refreshTaskResetResources();

    TEST_ASSERT_TRUE_MESSAGE(initOk, "refreshTaskInit return value");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserve0, "reserving frame 0");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserve1, "reserving frame 1");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, reserve2, "reserving frame 2");
    TEST_ASSERT_EQUAL_MESSAGE(0, out1.framesToRelease.len, "first frame should not release anything");
    TEST_ASSERT_EQUAL_MESSAGE(0, out2.framesToRelease.len, "second frame should only buffer, not release");
    TEST_ASSERT_EQUAL_MESSAGE(1, out3.framesToRelease.len, "third frame should release the superseded second frame");
    TEST_ASSERT_EQUAL_MESSAGE(ndx1, out3.framesToRelease.list[0].index, "released frame should be the superseded index");
    TEST_ASSERT_EQUAL_MESSAGE(availableBefore + 1, availableAfter, "released frame should be returned to the pool");
    TEST_ASSERT_FALSE_MESSAGE(releasedStillReserved, "released frame should no longer be marked reserved");
}

/* ---- end-to-end hardware tests: verify refreshTaskHandleCommand()'s
   dispatch of FSM actions actually reaches real led_matrix hardware, not
   just that the right RefreshFSMAction is returned. Frame slots are
   reserved and populated via refreshTaskReserveFrame() (as in the tests above), then
   fed back into refreshTaskHandleCommand() directly with hand-built
   commands so the FSM state machine can be driven deterministically
   through WAITING_FOR_FRAMES -> INSTALLING_FRAME -> CLEARING_FRAME
   without a running task or command queue involved. ---- */

TEST_CASE("refreshTaskHandleCommand_installThenNightMode_writesRealColorsToLedMatrix", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    /* refreshTaskInit() calls matReset(), so the board is known-blank
       going into this test */
    bool initOk = refreshTaskInit();

    /* a single live LED, well below its typical speed so it unambiguously
       classifies as "slow" (< 50% of typical) regardless of where the
       exact cutoff boundary falls */
    LEDSpeed (*currFrame)[MAX_FRAME_SIZE];
    uint32_t currNdx = UINT32_MAX;
    esp_err_t reserveCurr = refreshTaskReserveFrame(&currFrame, &currNdx, 0);
    if (ESP_OK == reserveCurr)
    {
        (*currFrame)[0].ledNum = TEST_LED_NUM;
        (*currFrame)[0].speed = 20;
    }

    /* typical frames are indexed by LED number (frame[ledNum].ledNum ==
       ledNum) for every slot from 0 to MAX_NUM_LEDS_REG, matching the
       lookup the FSM performs in ledSpeedToColor() */
    LEDSpeed (*typicalNorth)[MAX_FRAME_SIZE];
    LEDSpeed (*typicalSouth)[MAX_FRAME_SIZE];
    uint32_t typicalNorthNdx = UINT32_MAX, typicalSouthNdx = UINT32_MAX;
    esp_err_t reserveTypicalNorth = refreshTaskReserveFrame(&typicalNorth, &typicalNorthNdx, 0);
    esp_err_t reserveTypicalSouth = refreshTaskReserveFrame(&typicalSouth, &typicalSouthNdx, 0);
    if (ESP_OK == reserveTypicalNorth && ESP_OK == reserveTypicalSouth)
    {
        for (uint32_t i = 0; i <= MAX_NUM_LEDS_REG; i++)
        {
            (*typicalNorth)[i].ledNum = i;
            (*typicalNorth)[i].speed = 100;
            (*typicalSouth)[i].ledNum = i;
            (*typicalSouth)[i].speed = 100;
        }
    }

    /* while only the current frame is known, the FSM stays in
       WAITING_FOR_FRAMES and takes no action */
    RefreshFSMCommand cmdCurr = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = currNdx, .frameLen = 1, .dir = NORTH };
    RefreshFSMOutput outCurr;
    refreshTaskHandleCommand(&cmdCurr, &outCurr);

    /* the north typical frame alone still isn't enough; south is still missing */
    RefreshFSMCommand cmdTypicalNorth = { .type = REFRESH_CMD_UPDATE_TYPICAL, .frameNdx = typicalNorthNdx, .frameLen = MAX_NUM_LEDS_REG + 1, .dir = NORTH };
    RefreshFSMOutput outTypicalNorth;
    refreshTaskHandleCommand(&cmdTypicalNorth, &outTypicalNorth);

    /* south typical frame is the last piece: the FSM should immediately
       install the single-LED current frame, which means
       refreshTaskHandleCommand() has already dispatched a real
       REFRESH_ACTION_SET to led_matrix by the time this call returns */
    RefreshFSMCommand cmdTypicalSouth = { .type = REFRESH_CMD_UPDATE_TYPICAL, .frameNdx = typicalSouthNdx, .frameLen = MAX_NUM_LEDS_REG + 1, .dir = SOUTH };
    RefreshFSMOutput outTypicalSouth;
    refreshTaskHandleCommand(&cmdTypicalSouth, &outTypicalSouth);

    uint8_t installedRed = 0, installedGreen = 0, installedBlue = 0;
    esp_err_t readInstalled = matGetColor(TEST_LED_NUM, &installedRed, &installedGreen, &installedBlue);

    /* turning night mode on while a frame is installed should clear the
       single lit LED in this same tick, dispatching a real
       REFRESH_ACTION_CLEAR to led_matrix */
    RefreshFSMCommand cmdNightModeOn = { .type = REFRESH_CMD_NIGHT_MODE_ON, .frameNdx = 0, .frameLen = 0, .dir = NO_DIR };
    RefreshFSMOutput outNightModeOn;
    refreshTaskHandleCommand(&cmdNightModeOn, &outNightModeOn);

    uint8_t clearedRed = 0, clearedGreen = 0, clearedBlue = 0;
    esp_err_t readCleared = matGetColor(TEST_LED_NUM, &clearedRed, &clearedGreen, &clearedBlue);

    refreshTaskResetResources();

    TEST_ASSERT_TRUE_MESSAGE(initOk, "refreshTaskInit return value");
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

/* Regression test: REFRESH_ACTION_CLEAR_RANGE must look up the typical
   frame by LED number (typical[speed.ledNum]), not by position in the live
   frame (typical[ndx]). The live frame uses LED numbers that differ from
   their positions, and the skipped LED is painted with a sentinel color
   that the quick clear must leave alone. */
TEST_CASE("refreshTaskHandleCommand_quickClear_skipsLEDsByTypicalSpeedOfLEDNumber", TEST_GROUP)
{
    esp_err_t setupErr = setUpResourcesWithoutRunningTask();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, setupErr, "resource setup");

    /* refreshTaskInit() calls matReset(), so the board is known-blank */
    bool initOk = refreshTaskInit();

    /* pick three valid LED numbers >= 3, so none equals its frame position
       (0..2) and the skipped LED's typical slot isn't one the buggy
       position lookup would read */
    uint16_t ledNums[3] = { 0 };
    uint32_t found = 0;
    for (uint16_t ledNum = 3; ledNum <= MAX_NUM_LEDS_REG && found < 3; ledNum++)
    {
        if (isLEDValid(LEDNumToReg[ledNum - 1])) ledNums[found++] = ledNum;
    }
    const uint16_t ledA = ledNums[0], ledB = ledNums[1], ledC = ledNums[2];

    LEDSpeed (*currFrame)[MAX_FRAME_SIZE], (*typicalNorth)[MAX_FRAME_SIZE], (*typicalSouth)[MAX_FRAME_SIZE];
    LEDSpeed (*frameX)[MAX_FRAME_SIZE], (*frameY)[MAX_FRAME_SIZE];
    uint32_t currNdx = UINT32_MAX, typicalNorthNdx = UINT32_MAX, typicalSouthNdx = UINT32_MAX;
    uint32_t xNdx = UINT32_MAX, yNdx = UINT32_MAX;
    esp_err_t reserveCurr = refreshTaskReserveFrame(&currFrame, &currNdx, 0);
    esp_err_t reserveTypicalNorth = refreshTaskReserveFrame(&typicalNorth, &typicalNorthNdx, 0);
    esp_err_t reserveTypicalSouth = refreshTaskReserveFrame(&typicalSouth, &typicalSouthNdx, 0);
    esp_err_t reserveX = refreshTaskReserveFrame(&frameX, &xNdx, 0);
    esp_err_t reserveY = refreshTaskReserveFrame(&frameY, &yNdx, 0);
    bool reservedAll = (ESP_OK == reserveCurr && ESP_OK == reserveTypicalNorth && ESP_OK == reserveTypicalSouth &&
                        ESP_OK == reserveX && ESP_OK == reserveY);

    if (reservedAll && 3 == found)
    {
        (*currFrame)[0] = (LEDSpeed) { .ledNum = ledA, .speed = 20 };
        (*currFrame)[1] = (LEDSpeed) { .ledNum = ledB, .speed = 20 };
        (*currFrame)[2] = (LEDSpeed) { .ledNum = ledC, .speed = 20 };
        for (uint32_t i = 0; i <= MAX_NUM_LEDS_REG; i++)
        {
            (*typicalNorth)[i] = (LEDSpeed) { .ledNum = i, .speed = 100 };
            (*typicalSouth)[i] = (LEDSpeed) { .ledNum = i, .speed = 100 };
        }
        (*typicalNorth)[ledB].speed = UINT8_MAX;
    }

    /* the skipped LED is never set by the FSM; paint it so a wrongful clear
       is detectable */
    esp_err_t paintErr = matSetColor(ledB, 0x12, 0x34, 0x56);

    /* install: A is set, B is skipped, C is set (frame fully installed) */
    RefreshFSMCommand cmdCurr = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = currNdx, .frameLen = 3, .dir = NORTH };
    RefreshFSMCommand cmdTypicalNorth = { .type = REFRESH_CMD_UPDATE_TYPICAL, .frameNdx = typicalNorthNdx, .frameLen = MAX_NUM_LEDS_REG + 1, .dir = NORTH };
    RefreshFSMCommand cmdTypicalSouth = { .type = REFRESH_CMD_UPDATE_TYPICAL, .frameNdx = typicalSouthNdx, .frameLen = MAX_NUM_LEDS_REG + 1, .dir = SOUTH };
    RefreshFSMCommand cmdNone = { .type = REFRESH_CMD_NONE, .frameNdx = UINT32_MAX, .frameLen = UINT32_MAX, .dir = NO_DIR };
    RefreshFSMOutput outCurr, outTypicalNorth, outInstallA, outInstallC;
    refreshTaskHandleCommand(&cmdCurr, &outCurr);
    refreshTaskHandleCommand(&cmdTypicalNorth, &outTypicalNorth);
    refreshTaskHandleCommand(&cmdTypicalSouth, &outInstallA);
    refreshTaskHandleCommand(&cmdNone, &outInstallC);

    /* frame X starts a one-LED-per-tick clear (C first); frame Y then
       interrupts it, so A and B are handled by a single CLEAR_RANGE */
    RefreshFSMCommand cmdX = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = xNdx, .frameLen = 0, .dir = NORTH };
    RefreshFSMCommand cmdY = { .type = REFRESH_CMD_NEW_FRAME, .frameNdx = yNdx, .frameLen = 0, .dir = NORTH };
    RefreshFSMOutput outClearC, outQuickClear;
    refreshTaskHandleCommand(&cmdX, &outClearC);
    refreshTaskHandleCommand(&cmdY, &outQuickClear);

    uint8_t aRed = 0xFF, aGreen = 0xFF, aBlue = 0xFF;
    uint8_t bRed = 0, bGreen = 0, bBlue = 0;
    uint8_t cRed = 0xFF, cGreen = 0xFF, cBlue = 0xFF;
    esp_err_t readA = matGetColor(ledA, &aRed, &aGreen, &aBlue);
    esp_err_t readB = matGetColor(ledB, &bRed, &bGreen, &bBlue);
    esp_err_t readC = matGetColor(ledC, &cRed, &cGreen, &cBlue);

    refreshTaskResetResources();

    TEST_ASSERT_TRUE_MESSAGE(initOk, "refreshTaskInit return value");
    TEST_ASSERT_EQUAL_MESSAGE(3, found, "need three valid LED numbers >= 3");
    TEST_ASSERT_TRUE_MESSAGE(reservedAll, "reserving frames");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, paintErr, "painting sentinel on skipped LED");

    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outInstallA.action.type, "install first LED");
    TEST_ASSERT_EQUAL_MESSAGE(ledA, outInstallA.action.set.ledNum, "install first LED");
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_SET, outInstallC.action.type, "install skips B");
    TEST_ASSERT_EQUAL_MESSAGE(ledC, outInstallC.action.set.ledNum, "install skips B");
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR, outClearC.action.type, "slow clear of C");
    TEST_ASSERT_EQUAL_MESSAGE(ledC, outClearC.action.clear.ledNum, "slow clear of C");
    TEST_ASSERT_EQUAL_MESSAGE(REFRESH_ACTION_CLEAR_RANGE, outQuickClear.action.type, "quick clear of A and B");
    TEST_ASSERT_EQUAL_MESSAGE(typicalNorthNdx, outQuickClear.action.clearRange.typicalFrameNdx, "quick clear uses the NORTH typical frame");

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, readA, "matGetColor A");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, aRed, "A cleared by quick clear (red)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, aGreen, "A cleared by quick clear (green)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, aBlue, "A cleared by quick clear (blue)");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, readB, "matGetColor B");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x12, bRed, "skipped LED B untouched by quick clear (red)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x34, bGreen, "skipped LED B untouched by quick clear (green)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x56, bBlue, "skipped LED B untouched by quick clear (blue)");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, readC, "matGetColor C");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, cRed, "C cleared (red)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, cGreen, "C cleared (green)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, cBlue, "C cleared (blue)");
}
