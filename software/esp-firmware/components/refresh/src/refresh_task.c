/**
 * refresh_task.c
 * 
 * Contains a task that controls traffic LED updates and
 * handles high level commands to update the board with a
 * new frame of LEDs. It sleeps between updating each LED 
 * and wakes up if a new command is received.
 * 
 * In the case that a new command requires changing the board
 * frame from that of the current command, the task will abort
 * the current command.
 */

#include "refresh_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "app_err.h"
#include "indicators.h"
#include "main_types.h"
#include "led_registers.h"
#include "led_matrix.h"
#include "led_types.h"
#include "utilities.h"

#include "refresh_config.h"
#include "refresh_fsm.h"
#include "refresh_types.h"
#include "refresh_utilities.h"

#define TAG "refreshTask"

#define UNUSED(x)   (void)(x)

/* The delay between LED updates in milliseconds */
#define LED_UPDATE_DELAY_MS     (2)

#define NUM_RETRY               (5)
/* The number of LED frames that can be reserved */
#define NUM_LED_FRAMES          (6)

/* A queue holding RefreshFSMCommands which the refresh task will handle in order */
static QueueHandle_t commandQueue = NULL; 
/* frames of LEDs for commands to pass LED data to the refresh task */
static LEDSpeed ledFrames[NUM_LED_FRAMES][MAX_FRAME_SIZE];
/* true if the corresponding index in ledFrames is currently reserved. 
 * Only used for confirming indices can be released */
static bool ledFrameReserved[NUM_LED_FRAMES];
/* a queue of uint32_t ledFrames indices that are not currently reserved */
static QueueHandle_t ledFramesEmptyNdxQueue = NULL;
/* The LED register lookup table given to the FSM, which indexes it directly
 * by LED number. LEDNumToReg describes LED number i + 1 at index i, so this
 * is LEDNumToReg shifted up by one with an invalid entry for LED number 0. */
static LEDReg fsmLEDNumToReg[MAX_NUM_LEDS_REG + 1];

/* task notification values the refresh task sends its creator after init */
#define INIT_NOTIFY_SUCCESS     (1)
#define INIT_NOTIFY_FAILURE     (2)

static esp_err_t reserveLEDFrame(uint32_t *ndx, TickType_t ticksToWait);
static esp_err_t releaseLEDFrame(uint32_t ndx);
static void refreshTask(void *params);
static esp_err_t handleRefreshFSMAction(RefreshFSMAction *action);
static esp_err_t createTaskResources(void);
STATIC_IF_NOT_TEST bool refreshTaskInit(void);
STATIC_IF_NOT_TEST void refreshTaskHandleCommand(RefreshFSMCommand *cmd, RefreshFSMOutput *out);

/**
 * Creates commandQueue and ledFramesEmptyNdxQueue, populating
 * ledFramesEmptyNdxQueue with every valid frame index.
 *
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if commandQueue already exists.
 * ESP_ERR_NO_MEM if memory could not be allocated for resources.
 * ESP_FAIL if a logical error occurred.
 */
static esp_err_t createTaskResources(void)
{
    BaseType_t success;
    if (NULL != commandQueue)
    {
        return ESP_ERR_INVALID_STATE;
    }

    commandQueue = xQueueCreate(NUM_LED_FRAMES, sizeof(RefreshFSMCommand));
    if (NULL == commandQueue) return ESP_ERR_NO_MEM;
    ledFramesEmptyNdxQueue = xQueueCreate(NUM_LED_FRAMES, sizeof(uint32_t));
    if (NULL == ledFramesEmptyNdxQueue)
    {
        vQueueDelete(commandQueue);
        commandQueue = NULL;
        return ESP_ERR_NO_MEM;
    }

    for (uint32_t i = 0; i < NUM_LED_FRAMES; i++)
    {
        success = xQueueSend(ledFramesEmptyNdxQueue, &i, 0);
        if (success != pdPASS)
        {
            vQueueDelete(commandQueue);
            commandQueue = NULL;
            vQueueDelete(ledFramesEmptyNdxQueue);
            ledFramesEmptyNdxQueue = NULL;
            return ESP_FAIL;
        }
    }
    for (uint32_t i = 0; i < NUM_LED_FRAMES; i++)
    {
        ledFrameReserved[i] = false;
    }

    return ESP_OK;
}

/**
 * Creates the refresh task and initializes resources, blocking until
 * the task has finished initializing the LED matrices.
 *
 * @note The task resets the LED matrices during initialization, which
 * blanks every LED. Blocking here lets the caller set other LEDs (e.g.
 * indicators) afterwards without that reset racing them.
 *
 * @note Uses the calling task's notification (index 0) to wait for
 * the refresh task, clearing any notification already pending on it.
 *
 * @param[out] handle Where a handle to the created task
 * is created if successful. Can be NULL.
 * @param[in] prio The priority of the refresh task.
 *
 * @returns ESP_OK if the task was created and initialized successfully.
 * ESP_ERR_INVALID_STATE if the task was already created.
 * ESP_ERR_NO_MEM if memory could not be allocated for resources.
 * ESP_FAIL if the task could not be created or failed to initialize.
 */
esp_err_t createRefreshTask(TaskHandle_t *handle, const UBaseType_t prio)
{
    uint32_t initResult = 0;

    esp_err_t err = createTaskResources();
    if (ESP_OK != err) return err;

    TaskHandle_t creator = xTaskGetCurrentTaskHandle();
    (void) xTaskNotifyStateClear(creator);
    (void) ulTaskNotifyValueClear(creator, UINT32_MAX);

    BaseType_t success = xTaskCreate(refreshTask, "RefreshTask", 15000, (void *) creator, prio, handle);
    if (pdPASS != success) return ESP_FAIL;

    (void) xTaskNotifyWait(0, UINT32_MAX, &initResult, portMAX_DELAY);
    return (INIT_NOTIFY_SUCCESS == initResult) ? ESP_OK : ESP_FAIL;
}

/**
 * Reserves an LED frame which can then be used to call other refresh_task.h
 * functions.
 * 
 * @param[out] frame The location of the frame that was reserved. NULL if failed.
 * @param[out] frameNdx The frame index in ledFrames of the index that was
 * reserved. This will be provided to refresh_task.h functions when the frame
 * is ready to send back to the refresh task. UINT32_MAX if failed.
 * @param[in] ticksToWait The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 * ESP_FAIL if a logical error occurred.
 */
esp_err_t refreshTaskReserveFrame(LEDSpeed (**frame)[MAX_FRAME_SIZE], uint32_t *frameNdx, TickType_t ticksToWait)
{
    esp_err_t err;

    if (NULL == frame) return ESP_ERR_INVALID_ARG;
    if (NULL == frameNdx) return ESP_ERR_INVALID_ARG;

    err = reserveLEDFrame(frameNdx, ticksToWait);
    if (ESP_OK != err) return err;
    
    *frame = &ledFrames[*frameNdx];
    return ESP_OK;
}

/**
 * Releases an LED frame that was reserved with refreshTaskReserveFrame.
 * 
 * @note This function should only be used if something went wrong and
 * the reason for reserving the frame could not be completed.
 * 
 * @param[in] frameNdx The frame index of the reserved frame.
 * @param[in] ticksToWait The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if the frame was not previously reserved.
 * ESP_ERR_TIMEOUT if block time exceeded while releasing the frame.
 */
esp_err_t refreshTaskReleaseFrame(uint32_t frameNdx, TickType_t ticksToWait)
{
    if (frameNdx >= NUM_LED_FRAMES) return ESP_ERR_INVALID_ARG;
    if (!ledFrameReserved[frameNdx]) return ESP_ERR_INVALID_ARG;
    ledFrameReserved[frameNdx] = false; // must happen before queueing
    BaseType_t success = xQueueSend(ledFramesEmptyNdxQueue, &frameNdx, ticksToWait);
    if (pdPASS != success)
    {
        ledFrameReserved[frameNdx] = true;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/**
 * Sends a command to the refresh task to install a new LED frame
 * to the board.
 * 
 * @note LEDs are updated in the order of the frame.
 * 
 * @note Delays can be added by placing invalid LED numbers in
 * the data array.
 * 
 * @requires:
 * - createRefreshTask called.
 * 
 * @param frameNdx The frame index of the frame previously reserved
 * with refreshTaskReserveFrame and filled with LED speeds. The reservation
 * on this frame is consumed by this function. The caller no longer owns
 * the data in the frame.
 * @param frameLen The length of the frame stored in the frame reserved
 * at frameNdx.
 * @param dir The direction to refresh LEDs in.
 * @param received True when the queued command has been received. This is 
 * useful to avoid adding queueing more than one of a particular command
 * type at once. Set to NULL if unused.
 * @param cleared True when the previously installed frame has finished being
 * cleared.
 * @param ticksToWait The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 * ESP_FAIL if a logical error occurred.
 */
esp_err_t refreshTaskRefreshLEDs(uint32_t frameNdx, uint32_t frameLen, Direction dir, volatile bool *received, volatile bool *cleared, TickType_t ticksToWait)
{
    if (frameNdx >= NUM_LED_FRAMES) return ESP_ERR_INVALID_ARG;
    if (frameLen > MAX_FRAME_SIZE) return ESP_ERR_INVALID_ARG;
    if (NO_DIR == dir) return ESP_ERR_INVALID_ARG;

    RefreshFSMCommand cmd = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameNdx,
        .frameLen = frameLen,
        .dir = dir,
        .received = received,
        .cleared = cleared,
    };

    /* reset before queueing, since the refresh task may set them first */
    const bool prevReceived = (NULL != received) ? *received : false;
    const bool prevCleared = (NULL != cleared) ? *cleared : false;
    if (NULL != received) *received = false;
    if (NULL != cleared) *cleared = false;
    BaseType_t success = xQueueSend(commandQueue, &cmd, ticksToWait);
    if (pdPASS != success)
    {
        /* the command was never queued */
        if (NULL != received) *received = prevReceived;
        if (NULL != cleared) *cleared = prevCleared;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/**
 * Sends a command to the refresh task to install a new LED frame
 * for typical data.
 *
 * @note The typical data set in the frame must obey the following:
 * - every index from 0 to MAX_NUM_LEDS_REG must be written and each
 * index between them must satisfy frame[n].ledNum == n. Index 0 is unused
 * as LED indexing starts at 1. Unused LEDs and non-traffic LEDs must
 * also obey this rule.
 *
 * @requires:
 * - createRefreshTask called.
 *
 * @param frameNdx The frame index of the frame previously reserved
 * with refreshTaskReserveFrame and filled with typical LED speeds. The
 * reservation on this frame is consumed by this function. The caller
 * no longer owns the data in the frame, unless this function failed.
 * @param frameLen The length of the frame stored in the frame reserved
 * at frameNdx. Should be exactly MAX_NUM_LEDS_REG + 1.
 * @param dir The traffic direction the typical speeds correspond to.
 * @param received True when the queued command has been received. This is 
 * useful to avoid adding queueing more than one of a particular command
 * type at once. Set to NULL if unused.
 * @param ticksToWait The maximum number of ticks to block for.
 *
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 * ESP_FAIL if a logical error occurred.
 */
esp_err_t refreshTaskUpdateTypical(uint32_t frameNdx, uint32_t frameLen, Direction dir, volatile bool *received, TickType_t ticksToWait)
{
    if (frameNdx >= NUM_LED_FRAMES) return ESP_ERR_INVALID_ARG;
    if (frameLen != MAX_NUM_LEDS_REG + 1) return ESP_ERR_INVALID_ARG;
    if (NO_DIR == dir) return ESP_ERR_INVALID_ARG;

    RefreshFSMCommand cmd = {
        .type = REFRESH_CMD_UPDATE_TYPICAL,
        .frameNdx = frameNdx,
        .frameLen = frameLen,
        .dir = dir,
        .received = received,
        .cleared = NULL,
    };

    /* reset before queueing, since the refresh task may set it first */
    const bool prevReceived = (NULL != received) ? *received : false;
    if (NULL != received) *received = false;
    BaseType_t success = xQueueSend(commandQueue, &cmd, ticksToWait);
    if (pdPASS != success)
    {
        /* the command was never queued */
        if (NULL != received) *received = prevReceived;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/**
 * Sends a command to the refresh task to enable night mode.
 * 
 * @note Night mode turns off traffic and direction indicator
 * LEDs until night mode is disabled.
 * 
 * @param received True when the queued command has been received. This is 
 * useful to avoid adding queueing more than one of a particular command
 * type at once. Set to NULL if unused.
 * @param ticksToWait The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 */
esp_err_t refreshTaskEnableNightMode(volatile bool *received, TickType_t ticksToWait)
{
    RefreshFSMCommand cmd = {
        .type = REFRESH_CMD_NIGHT_MODE_ON,
        .frameNdx = MAX_NUM_LEDS_REG,
        .frameLen = 0,
        .dir = NO_DIR,
        .received = received,
        .cleared = NULL,
    };

    /* reset before queueing, since the refresh task may set it first */
    const bool prevReceived = (NULL != received) ? *received : false;
    if (NULL != received) *received = false;
    BaseType_t success = xQueueSend(commandQueue, &cmd, ticksToWait);
    if (pdPASS != success)
    {
        /* the command was never queued */
        if (NULL != received) *received = prevReceived;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/**
 * Sends a command to the refresh task to disable night mode.
 * 
 * @note Night mode turns off traffic and direction indicator
 * LEDs until night mode is disabled.
 * 
 * @param received True when the queued command has been received. This is 
 * useful to avoid adding queueing more than one of a particular command
 * type at once. Set to NULL if unused.
 * @param ticksToWait The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 */
esp_err_t refreshTaskDisableNightMode(volatile bool *received, TickType_t ticksToWait)
{
    RefreshFSMCommand cmd = {
        .type = REFRESH_CMD_NIGHT_MODE_OFF,
        .frameNdx = MAX_NUM_LEDS_REG,
        .frameLen = 0,
        .dir = NO_DIR,
        .received = received,
        .cleared = NULL,
    };

    /* reset before queueing, since the refresh task may set it first */
    const bool prevReceived = (NULL != received) ? *received : false;
    if (NULL != received) *received = false;
    BaseType_t success = xQueueSend(commandQueue, &cmd, ticksToWait);
    if (pdPASS != success)
    {
        /* the command was never queued */
        if (NULL != received) *received = prevReceived;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/**
 * Reserves an LED frame for a command.
 * 
 * @requires:
 * - createRefreshTask called.
 * 
 * @param ndx A pointer to where the index of the
 * LED frame in the ledFrames array will be stored.
 * @param ticksToWait The maximum number of ticks to wait.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 * ESP_FAIL if a logical error occurred.
 */
static esp_err_t reserveLEDFrame(uint32_t *ndx, TickType_t ticksToWait)
{
    if (NULL == ndx) return ESP_ERR_INVALID_ARG;

    uint32_t frameNdx = NUM_LED_FRAMES;
    BaseType_t success = xQueueReceive(ledFramesEmptyNdxQueue, &frameNdx, ticksToWait);
    if (pdPASS != success) return ESP_ERR_TIMEOUT;
    if (frameNdx >= NUM_LED_FRAMES) return ESP_FAIL;
    if (ledFrameReserved[frameNdx]) return ESP_FAIL; // dont put the frame back on the queue
    ledFrameReserved[frameNdx] = true;
    *ndx = frameNdx;
    return ESP_OK;
}

/**
 * Releases an LED frame for use by a new command.
 * 
 * @requires:
 * - createRefreshTask called.
 * 
 * @param ndx The index of the LED frame to release.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_FAIL if unable to release LED frame.
 */
static esp_err_t releaseLEDFrame(uint32_t ndx)
{
    if (ndx >= NUM_LED_FRAMES) return ESP_ERR_INVALID_ARG;

    ledFrameReserved[ndx] = false; // must happen before queueing
    BaseType_t success = xQueueSend(ledFramesEmptyNdxQueue, &ndx, 0);
    if (pdTRUE != success)
    {
        ledFrameReserved[ndx] = true;
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * Implements the refresh task, which is responsible for
 * handling traffic LED commands.
 * 
 * @param params The handle of the task that created this task, which
 * is notified with the result of initialization.
 */
static void refreshTask(void *params)
{
    TaskHandle_t creator = (TaskHandle_t) params;
    TickType_t sleepTicks = portMAX_DELAY;

    bool initialized = refreshTaskInit();
    (void) xTaskNotify(creator,
                       initialized ? INIT_NOTIFY_SUCCESS : INIT_NOTIFY_FAILURE,
                       eSetValueWithOverwrite);
    if (!initialized)
    {
        vTaskDelete(NULL);
    }

    while (true)
    {
        RefreshFSMCommand cmd;
        RefreshFSMOutput out;

        /* sleep while waiting for command */
        BaseType_t success = xQueueReceive(commandQueue, (void *) &cmd, sleepTicks);
        if (pdTRUE != success)
        {
            cmd = (RefreshFSMCommand) { .type = REFRESH_CMD_NONE };
        }

        refreshTaskHandleCommand(&cmd, &out);
        sleepTicks = (out.isIdle) ? portMAX_DELAY : pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD);
    }

    ESP_LOGE(TAG, "Refresh task is exiting unexpectedly.");
    vTaskDelete(NULL);
}

/**
 * Runs initialization of dependencies for the refreshTask.
 * This is separated to reduce lifetime stack usage of the
 * refreshTask.
 * 
 * @param params The params provided to the refresh task, checked
 * for validity in this function.
 * 
 * @returns True if initialized successfully. False otherwise,
 * indicating the task should be deleted.
 */
STATIC_IF_NOT_TEST bool refreshTaskInit(void)
{
    const RefreshFSMResources fsmResources = {
        .LEDNumToReg = fsmLEDNumToReg,
        .LEDNumToRegLen = MAX_NUM_LEDS_REG + 1,
        .LEDFrames = ledFrames,
        .LEDFramesLen = NUM_LED_FRAMES,
        .slowLEDColor = {
            .red = SLOW_RED,
            .green = SLOW_GREEN,
            .blue = SLOW_BLUE,
        },
        .mediumLEDColor = {
            .red = MEDIUM_RED,
            .green = MEDIUM_GREEN,
            .blue = MEDIUM_BLUE,
        },
        .fastLEDColor = {
            .red = FAST_RED,
            .green = FAST_GREEN,
            .blue = FAST_BLUE,
        },
    };
    esp_err_t err;

    if (NULL == commandQueue || NULL == ledFramesEmptyNdxQueue)
    {
        ESP_LOGE(TAG, "Refresh task resources are invalid.");
        return false;
    }

    err = initLedMatrix();
    if (ESP_OK != err && ESP_ERR_INVALID_STATE != err)
    {
        ESP_LOGE(TAG, "LED Matrices could not be initialized: %d", err);
        return false;
    }

    err = matReset();
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "LED matrices could not be reset: %d", err);
        return false;
    }

    /* matReset returns the matrices to their power-on defaults: software
    shutdown, zero global current, and zero scaling on every LED. Restore
    each of these or nothing will light up. */
#if CONFIG_HARDWARE_VERSION == 1
    err = matSetGlobalCurrentControl(CONFIG_GLOBAL_LED_CURRENT);
#elif CONFIG_HARDWARE_VERSION == 2
    err = matSetGCCByAmbientLight();
#else
#error "Unsupported hardware version!"
#endif
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "LED matrix global current could not be set: %d", err);
        return false;
    }

    err = matSetOperatingMode(NORMAL_OPERATION);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "LED matrices could not be set to normal operation: %d", err);
        return false;
    }

    /* LEDNumToReg[i] describes LED number i + 1 */
    for (uint16_t ledNum = 1; ledNum <= MAX_NUM_LEDS_REG; ledNum++)
    {
        if (!isLEDValid(LEDNumToReg[ledNum - 1])) continue;
        for (uint32_t i = 0; i < MATRIX_RETRY_NUM; i++)
        {
            err = matSetScaling(ledNum, DEFAULT_SCALE, DEFAULT_SCALE, DEFAULT_SCALE);
            if (ESP_OK == err) break;
        }
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "LED %u scaling could not be set: %d", ledNum, err);
            return false;
        }
    }

    /* the FSM indexes its lookup table directly by LED number */
    fsmLEDNumToReg[0] = (LEDReg) { .matrix = MAT_NONE };
    for (uint16_t ledNum = 1; ledNum <= MAX_NUM_LEDS_REG; ledNum++)
    {
        fsmLEDNumToReg[ledNum] = LEDNumToReg[ledNum - 1];
    }

    refreshFSMInit(&fsmResources);
    return true;
}

/**
 * Handles the refresh FSM command and produces FSM output
 * 
 * @param cmd The refresh FSM command to handle.
 * @param out The location to place output of the refresh FSM.
 */
STATIC_IF_NOT_TEST void refreshTaskHandleCommand(RefreshFSMCommand *cmd, RefreshFSMOutput *out)
{
    esp_err_t err;
    
    /* provide command to refresh FSM and handle action */
    *out = refreshFSMTick(cmd);
    err = handleRefreshFSMAction(&out->action); // must process action before frames are released
    assert(ESP_OK == err);
    if (out->indicator.update)
    {
        /* after the action, so indicators change once the old frame is gone */
        err = indicateDirection(out->indicator.dir);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "Failed to set direction indicators to %d: %d", out->indicator.dir, err);
        }
    }
    if (0 != out->framesToRelease.len)
    {
        for (uint32_t i = 0; i < out->framesToRelease.len; i++)
        {
            ESP_LOGI(TAG, "releasing frame %d for reason %d", out->framesToRelease.list[i].index, out->framesToRelease.list[i].type);
            esp_err_t err = releaseLEDFrame(out->framesToRelease.list[i].index);
            assert(ESP_OK == err && "failed to release LED frame");

        }
    }

    /* only after any displaced frame is back in the pool, so a caller
       waiting on this can reserve a frame */
    if (REFRESH_CMD_NONE != cmd->type && NULL != cmd->received) *cmd->received = true;
}

/**
 * Performs the requested FSM action.
 * 
 * @param action The requested FSM action.
 * 
 * @returns ESP_OK if successful. Potentially other errors.
 */
static esp_err_t handleRefreshFSMAction(RefreshFSMAction *action)
{
    esp_err_t err = ESP_OK;
    const Color colorOff = { .red = 0x00, .blue = 0x00, .green = 0x00 };

    if (REFRESH_ACTION_NONE == action->type) return ESP_OK;

    switch (action->type)
    {
        case REFRESH_ACTION_SET:
            err = setLEDColor(action->set.ledNum, action->set.color, DONT_SET_BRIGHTNESS);
            if (ESP_OK != err)
            {
                ESP_LOGE(TAG, "Failed to set LED %d.", action->set.ledNum);
            }
            break;
        case REFRESH_ACTION_CLEAR:
            err = setLEDColor(action->clear.ledNum, colorOff, DONT_SET_BRIGHTNESS);
            if (ESP_OK != err)
            {
                ESP_LOGE(TAG, "Failed to clear LED %lu.", action->clear.ledNum);
            }
            break;
        case REFRESH_ACTION_CLEAR_RANGE:
            for (uint32_t ndx = action->clearRange.startNdx; ndx != UINT32_MAX; ndx--)
            {
                LEDSpeed speed = ledFrames[action->clearRange.frameNdx][ndx];
                /* LEDNumToReg[i] describes LED number i + 1 */
                if (0 == speed.ledNum || speed.ledNum > MAX_NUM_LEDS_REG) continue;
                if (ledFrames[action->clearRange.typicalFrameNdx][speed.ledNum].speed == UINT8_MAX) continue;
                if (!isLEDValid(LEDNumToReg[speed.ledNum - 1])) continue;

                err = setLEDColor(speed.ledNum, colorOff, DONT_SET_BRIGHTNESS);
                if (ESP_OK != err)
                {
                    ESP_LOGE(TAG, "Failed to clear LED %d.", speed.ledNum);
                }
            }
            break;
        case REFRESH_ACTION_NONE:
            ESP_LOGW(TAG, "REFRESH_ACTION_NONE found unexpectedly.");
            break;
    }

    return err;
}

#ifdef CONFIG_TEST_REFRESH

/**
 * Sets task resources to the state they
 * should be in during initialization, freeing
 * any queues created by a prior createRefreshTask().
 */
void refreshTaskResetResources(void)
{
    if (NULL != commandQueue)
    {
        vQueueDelete(commandQueue);
        commandQueue = NULL;
    }
    if (NULL != ledFramesEmptyNdxQueue)
    {
        vQueueDelete(ledFramesEmptyNdxQueue);
        ledFramesEmptyNdxQueue = NULL;
    }
}

/**
 * Creates commandQueue and ledFramesEmptyNdxQueue without spawning the
 * refresh task, so tests can call refreshTaskInit()/refreshTaskHandleCommand()
 * directly against real resources with no task ever created or scheduled.
 *
 * @returns ESP_OK if successful. See createTaskResources() for other codes.
 */
esp_err_t refreshTaskCreateResourcesForTest(void)
{
    return createTaskResources();
}

/**
 * @returns The number of LED frame slots currently available for
 * reservation. Exists so tests can observe ledFramesEmptyNdxQueue's
 * state directly, since refreshTaskRefreshLEDs() is not a reliable probe: it
 * consumes a commandQueue slot on every call too, and commandQueue
 * shares the same capacity, so its state can mask the frame pool's.
 */
uint32_t refreshTaskGetAvailableFrameCount(void)
{
    return (uint32_t) uxQueueMessagesWaiting(ledFramesEmptyNdxQueue);
}

/**
 * @returns True if the frame is reserved. False otherwise.
 */
bool refreshTaskIsFrameReserved(uint32_t frameNdx)
{
    if (frameNdx >= NUM_LED_FRAMES) return false;
    return ledFrameReserved[frameNdx];
}

#endif /* CONFIG_TEST_REFRESH */