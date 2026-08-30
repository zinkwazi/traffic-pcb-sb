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

#include "main_types.h"

#define TEST_GROUP "[refresh_task]"

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
