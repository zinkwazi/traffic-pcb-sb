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

#include "refresh_fsm.h"
#include "refresh_config.h"

#define TAG "refreshTask"

#define UNUSED(x)   (void)(x)

/* The delay between LED updates in milliseconds */
#define LED_UPDATE_DELAY_MS     (2)

#define NUM_RETRY               (5)

/* The length of commandQueue. */
#define COMMAND_QUEUE_LEN       (5)

/* The number of LED frames that can be reserved */
#define NUM_LED_FRAMES          (4)

/**
 * A command which the refresh task can handle.
 */
typedef struct {
    RefreshFSMCommandType type;
    uint32_t frameNdx; /* The ledFrames index where this command's LED frame is stored. NUM_LED_FRAMES if unused. */
    Direction dir; /* The animation and display direction. NO_DIR if unused. */
} RefreshCommand;

static QueueHandle_t commandQueue = NULL; /* A queue holding RefreshCommands which the refresh task will handle in order */

static LEDSpeed ledFrames[NUM_LED_FRAMES][MAX_FRAME_SIZE]; /* frames of LEDs for commands to pass LED data to the refresh task */
static QueueHandle_t ledFramesEmptyNdxQueue; /* a queue of uint32_t ledFrames indices that are not currently in use by a command */

static esp_err_t reserveLEDFrame(uint32_t *ndx, TickType_t ticksToWait);
static void refreshTask(void *params);

/**
 * Creates the refresh task and initializes resources.
 * 
 * @param[out] handle Where a handle to the created task
 * is created if successful. Can be NULL.
 * @param[in] prio The priority of the refresh task.
 * 
 * @requires:
 * - initLEDMatrix called.
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
    commandQueue = xQueueCreate(COMMAND_QUEUE_LEN, sizeof(RefreshCommand));
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
 * than or equal to 
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

    /* store LED frame for refresh task */
    uint32_t frameNdx;
    err = reserveLEDFrame(&frameNdx, blockTime);
    if (err != ESP_OK) return err;
    for (uint32_t i = 0; i < MAX_NUM_LEDS_REG; i++)
    {
        ledFrames[frameNdx][i] = data[i];
    }

    /* create and push command */
    RefreshCommand cmd = {
        .type = REFRESH_CMD_NEW_FRAME,
        .frameNdx = frameNdx,
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
    RefreshCommand cmd = {
        .type = REFRESH_CMD_NIGHT_MODE_ON,
        .frameNdx = MAX_NUM_LEDS_REG,
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
    RefreshCommand cmd = {
        .type = REFRESH_CMD_NIGHT_MODE_OFF,
        .frameNdx = MAX_NUM_LEDS_REG,
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
 * Implements the refresh task, which is responsible for
 * handling traffic LED commands.
 * 
 * @param params Task parameters. Unused.
 */
static void refreshTask(void *params)
{
    UNUSED(params);

    esp_err_t err;
    RefreshCommand nextCmd = { .type = REFRESH_CMD_NONE };

    /* check validity of resources */
    if (NULL == commandQueue || NULL == ledFramesEmptyNdxQueue)
    {
        ESP_LOGE(TAG, "Refresh task resources are invalid. Killing refresh task.");
        vTaskDelete(NULL);
    }

    /* initialize LED matrices */
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

    while (true)
    {

    }

    ESP_LOGE(TAG, "Refresh task is exiting unexpectedly.");
    vTaskDelete(NULL);
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