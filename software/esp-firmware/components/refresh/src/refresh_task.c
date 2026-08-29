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

static QueueHandle_t commandQueue = NULL; /* A queue holding RefreshFSMCommands which the refresh task will handle in order */

static LEDSpeed ledFrames[NUM_LED_FRAMES][MAX_FRAME_SIZE]; /* frames of LEDs for commands to pass LED data to the refresh task */
static QueueHandle_t ledFramesEmptyNdxQueue = NULL; /* a queue of uint32_t ledFrames indices that are not currently in use by a command */

static esp_err_t reserveLEDFrame(uint32_t *ndx, TickType_t ticksToWait);
static esp_err_t releaseLEDFrame(uint32_t ndx);
static void refreshTask(void *params);
static void initRefreshTask(void);
static esp_err_t handleRefreshFSMAction(RefreshFSMAction *action);

/**
 * Creates the refresh task and initializes resources.
 * 
 * @param[out] handle Where a handle to the created task
 * is created if successful. Can be NULL.
 * @param[in] prio The priority of the refresh task.
 * 
 * @returns ESP_OK if the task was created successfully.
 * ESP_ERR_INVALID_STATE if the task was already created.
 * ESP_ERR_NO_MEM if memory could not be allocated for resources.
 * ESP_FAIL if a logical error occurred.
 */
esp_err_t createRefreshTask(TaskHandle_t *handle, const UBaseType_t prio)
{
    BaseType_t success;
    if (NULL != commandQueue)
    {
        return ESP_ERR_INVALID_STATE;
    }
    
    /* initialize resources */
    commandQueue = xQueueCreate(NUM_LED_FRAMES, sizeof(RefreshFSMCommand));
    if (NULL == commandQueue) return ESP_ERR_NO_MEM;
    ledFramesEmptyNdxQueue = xQueueCreate(NUM_LED_FRAMES, sizeof(uint32_t));
    if (NULL == ledFramesEmptyNdxQueue)
    {
        vQueueDelete(commandQueue);
        return ESP_ERR_NO_MEM;
    }

    for (uint32_t i = 0; i < NUM_LED_FRAMES; i++)
    {
        success = xQueueSend(ledFramesEmptyNdxQueue, &i, 0);
        if (success != pdPASS)
        {
            vQueueDelete(commandQueue);
            vQueueDelete(ledFramesEmptyNdxQueue);
            return ESP_FAIL;
        }
    }

    /* create task */
    success = xTaskCreate(refreshTask, "RefreshTask", 15000, NULL, prio, handle);
    return (success == pdPASS) ? ESP_OK : ESP_FAIL;
}

/**
 * Sends a command to the refresh task to install a new LED frame
 * to the board.
 * 
 * @note LEDs are updated in the order presented by the data array.
 * 
 * @note Delays can be added by placing invalid LED numbers in
 * the data array.
 * 
 * @requires:
 * - createRefreshTask called.
 * 
 * @param data An array of LEDColor to use when refreshing LEDs.
 * Must be of size MAX_NUM_LEDS_REG.
 * @param dataLen The length of the data array. Must be less
 * than or equal to MAX_FRAME_SIZE.
 * @param dir The direction to refresh LEDs in.
 * @param blockTime The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 * ESP_FAIL if a logical error occurred.
 */
esp_err_t refreshLEDs(LEDSpeed *data, uint32_t dataLen, Direction dir, TickType_t blockTime)
{
    esp_err_t err;

    if (NULL == data) return ESP_ERR_INVALID_ARG;
    if (NO_DIR == dir) return ESP_ERR_INVALID_ARG;
    if (dataLen > MAX_FRAME_SIZE) return ESP_ERR_INVALID_ARG;

    /* store LED frame for refresh task */
    uint32_t frameNdx;
    err = reserveLEDFrame(&frameNdx, blockTime);
    if (err != ESP_OK) return err;
    for (uint32_t i = 0; i < dataLen; i++)
    {
        ledFrames[frameNdx][i] = data[i];
    }

    /* create and push command */
    RefreshFSMCommand cmd = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameNdx,
        .frameLen = dataLen,
        .dir = dir,
    };
    BaseType_t success = xQueueSend(commandQueue, &cmd, 0); // expect that if able to reserve a frame, that there is space on the command queue
    if (pdPASS != success)
    {
        /* unreserve LED frame */
        success = xQueueSend(ledFramesEmptyNdxQueue, &frameNdx, 0); // returning ESP_FAIL in both cases
        if (pdPASS != success) return ESP_FAIL;
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
 * @param blockTime The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 */
esp_err_t refreshEnableNightMode(TickType_t blockTime)
{
    RefreshFSMCommand cmd = {
        .type = REFRESH_CMD_NIGHT_MODE_ON,
        .frameNdx = MAX_NUM_LEDS_REG,
        .frameLen = 0,
        .dir = NO_DIR,
    };
    BaseType_t success = xQueueSend(commandQueue, &cmd, blockTime);
    if (pdPASS != success) return ESP_ERR_TIMEOUT;
    return ESP_OK;
}

/**
 * Sends a command to the refresh task to disable night mode.
 * 
 * @note Night mode turns off traffic and direction indicator
 * LEDs until night mode is disabled.
 * 
 * @param blockTime The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 */
esp_err_t refreshDisableNightMode(TickType_t blockTime)
{
    RefreshFSMCommand cmd = {
        .type = REFRESH_CMD_NIGHT_MODE_OFF,
        .frameNdx = MAX_NUM_LEDS_REG,
        .frameLen = 0,
        .dir = NO_DIR,
    };
    BaseType_t success = xQueueSend(commandQueue, &cmd, blockTime);
    if (pdPASS != success) return ESP_ERR_TIMEOUT;
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

    BaseType_t success = xQueueSend(ledFramesEmptyNdxQueue, &ndx, 0);
    if (pdTRUE != success)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * Implements the refresh task, which is responsible for
 * handling traffic LED commands.
 * 
 * @param params Task parameters. Unused.
 */
static void refreshTask(void *params)
{
    UNUSED(params);

    TickType_t sleepTicks = portMAX_DELAY;
    esp_err_t err;

    /* check validity of resources */
    if (NULL == commandQueue || NULL == ledFramesEmptyNdxQueue)
    {
        ESP_LOGE(TAG, "Refresh task resources are invalid. Killing refresh task.");
        vTaskDelete(NULL);
    }

    initRefreshTask();
    while (true)
    {
        RefreshFSMCommand cmd;
        RefreshFSMOutput out;

        /* sleep while waiting for command */
        BaseType_t success = xQueueReceive(commandQueue, (void *) &cmd, sleepTicks);
        if (pdTRUE != success)
        {
            cmd.type = REFRESH_CMD_NONE;
        }

        /* provide command to refresh FSM and handle action */
        out = refreshFSMTick(&cmd);
        err = handleRefreshFSMAction(&out.action); // must process action before frames are released
        assert(ESP_OK == err);
        if (0 != out.framesToRelease.len)
        {
            for (uint32_t i = 0; i < out.framesToRelease.len; i++)
            {
                ESP_LOGI(TAG, "releasing frame %d for reason %d", out.framesToRelease.list[i].index, out.framesToRelease.list[i].type);
                esp_err_t err = releaseLEDFrame(out.framesToRelease.list[i].index);
                assert(ESP_OK == err && "failed to release LED frame");

            }
        }
        sleepTicks = (out.isIdle) ? portMAX_DELAY : portTICK_PERIOD_MS * CONFIG_LED_UPDATE_PERIOD;
    }

    ESP_LOGE(TAG, "Refresh task is exiting unexpectedly.");
    vTaskDelete(NULL);
}

/**
 * Runs initialization of dependencies for the refreshTask.
 * This is separated to reduce lifetime stack usage of the
 * refreshTask.
 * 
 * @note if initialization fails, then this
 * function deletes the current task.
 */
static void initRefreshTask(void)
{
    const RefreshFSMResources fsmResources = {
        .LEDNumToReg = LEDNumToReg,
        .LEDNumToRegLen = MAX_NUM_LEDS_REG,
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

    err = initLedMatrix();
    if (ESP_OK != err && ESP_ERR_INVALID_STATE != err)
    {
        ESP_LOGE(TAG, "LED Matrices could not be initialized: %d", err);
        vTaskDelete(NULL);
    }

    err = matReset();
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "LED matrices could not be reset: %d", err);
        vTaskDelete(NULL);
    }

    refreshFSMInit(&fsmResources);
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
                ESP_LOGE(TAG, "Failed to clear LED %d.", action->set.ledNum);
            }
            break;
        case REFRESH_ACTION_CLEAR_RANGE:
            for (uint32_t ndx = action->clearRange.startNdx; ndx != UINT32_MAX; ndx--)
            {
                LEDSpeed speed = ledFrames[action->clearRange.frameNdx][ndx];
                if (speed.ledNum >= MAX_NUM_LEDS_REG) continue;
                if (!isLEDValid(LEDNumToReg[speed.ledNum])) continue;

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
 * should be in during initialization.
 */
void refreshResetTaskResources(void)
{
    commandQueue = NULL;
}

#endif /* CONFIG_TEST_REFRESH */