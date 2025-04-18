/**
 * strobe_task.c
 * 
 * Contains functionality to create the strobe task. Separate from strobe.h
 * in order to minimize the risk of modifying the strobe command queue handle.
 */

#include "strobe_task.h"
#include "strobe_types.h"

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "app_errors.h"
#include "led_matrix.h"
#include "led_registers.h"
#include "strobe.h"

#define TAG "strobe_task"

#ifdef CONFIG_SUPPORT_STROBING

/* The size in commands of the strobe command queue */
#define STROBE_QUEUE_SIZE (MAX_NUM_LEDS_REG) // must be able to strobe every LED in sync

/* The maximum number of LEDs the strobe task can handle simultaneously */
#define MAX_STROBE_LEDS (MAX_NUM_LEDS_REG)

/* The strobe period in milliseconds of strobing steps. Increasing this
   and increasing strobe steps will increase quantization at the same
   literal strobe period, while potentially fixing missed deadlines. */
#define STROBE_PERIOD (100)

/* The number of times to retry I2C transactions for scaling */
#define RETRY_I2C_NUM (5)


/* The strobe task command queue of size STROBE_QUEUE_SIZE, initialized by 
   createStrobeTask. Tasks will register and unregister led strobing by adding 
   commands to this queue. The queue should be accessed through getStrobeQueue
   in order to reduce the risk of accidentally overwriting this handle. */
static QueueHandle_t strobeQueue = NULL; // holds StrobeTaskCommand

/* protects strobeQueue, effectively stopping the strobe task from desynchronizing
a block of LEDs that should be strobed starting from particular values in order
to achieve an animation. The strobe task aquires this when reading from the queue. */
static SemaphoreHandle_t strobeQueueMutex = NULL;

/* An event group defining various strobe events */
static EventGroupHandle_t strobeEvents = NULL;

                        

static void vStrobeTask(void *pvParameters);
static int findStrobeLED(const StrobeLED strobeInfo[], int strobeInfoLen, uint16_t targetLED);
static void receiveCommand(StrobeLED strobeInfo[], int *strobeInfoLen, const StrobeTaskCommand *command);
static void strobeLEDs(StrobeLED strobeInfo[], const int strobeInfoLen);

/**
 * @brief Returns a copy of the strobeQueue handle. The strobeQueue holds
 *        StrobeTaskCommand objects.
 */
QueueHandle_t getStrobeQueue(void)
{
    return strobeQueue; // make sure that others cannot modify the base handle
}

/**
 * @brief wraps xSemaphoreTake, where the mutex to take is always strobeQueueMutex.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t acquireStrobeQueueMutex(TickType_t blockTime)
{
    return (xSemaphoreTake(strobeQueueMutex, blockTime) == pdPASS) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief wraps xSemaphoreGive, where the mutex to give is always strobeQueueMutex.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t releaseStrobeQueueMutex(void)
{
    return (xSemaphoreGive(strobeQueueMutex) == pdPASS) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Waits for strobe event group bits.
 * 
 * @param[in] bits The bits to wait for.
 * @param[in] allBits Whether to wait for all bits to be set, or any bit
 * to be set.
 * @param[in] blockTime The maximum amount of time to wait for. portMAX_DELAY
 * to wait forever.
 * 
 * @returns ESP_OK if successful and bits were set.
 * ESP_ERR_TIMEOUT if blockTime expired before bits were set (not thrown).
 */
esp_err_t strobeWaitEvent(EventBits_t bits, bool allBits, TickType_t blockTime)
{
    EventBits_t found = xEventGroupWaitBits(strobeEvents, bits, pdFALSE, (allBits) ? pdTRUE : pdFALSE, blockTime);
    if ((found & bits) != bits) return ESP_ERR_TIMEOUT;
    return ESP_OK;
}

/**
 * @brief Initializes the over-the-air (OTA) task, which is implemented by
 *        vOTATask.
 * 
 * @note This function creates shallow copies of parameters that will be 
 *       provided to the task in static memory. It assumes that only one of 
 *       this type of task will be created; any additional tasks will have 
 *       pointers to the same location in static memory.
 * 
 * @requires:
 * - app_errors component initialized.
 * 
 * @param[out] handle A pointer to a handle which will refer to the created task
 *        if successful. Optional--can be set to NULL.       
 * @param[in] errorResources A pointer to an ErrorResources object. A deep copy
 *        of the object will be created in static memory.
 *                       
 * @returns ESP_OK if the task was created successfully.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_NO_MEM if not enough freeRTOS dynamic memory was allocated.
 * ESP_FAIL if createStrobeTask was already called. In this case, recovery is 
 * not possible and the device must be restarted.
 */
esp_err_t createStrobeTask(TaskHandle_t *handle)
{
    BaseType_t success;

    /* input guards */
    if (getAppErrorsStatus() != ESP_OK) THROW_ERR(ESP_ERR_INVALID_STATE);

    /* initialize static variables */
    if (strobeQueue != NULL) return ESP_FAIL; // already initialized
    strobeQueue = xQueueCreate(STROBE_QUEUE_SIZE, sizeof(StrobeTaskCommand));
    if (strobeQueue == NULL) return ESP_ERR_NO_MEM;
    strobeQueueMutex = xSemaphoreCreateMutex();
    if (strobeQueueMutex == NULL) return ESP_ERR_NO_MEM;
    strobeEvents = xEventGroupCreate();
    if (strobeEvents == NULL) return ESP_ERR_NO_MEM;

    /* create OTA task */
    success = xTaskCreate(vStrobeTask, "StrobeTask", CONFIG_STROBE_STACK,
                            NULL, CONFIG_STROBE_PRIO, handle);
    if (success != pdPASS) THROW_ERR(ESP_ERR_NO_MEM);
    return ESP_OK;
}

/**
 * @brief Implements the strobe task, which is responsible for continuously
 *        strobing LEDs that other tasks have registered with it.
 * 
 * @note To avoid runtime errors, the strobe task should only be created by
 *       the createStrobeTask function.
 * 
 * @param[in] pvParameters A pointer to a StrobeTaskResources object which
 *        should remain valid through the lifetime of the task.
 */
static void vStrobeTask(void *pvParameters)
{
    const QueueHandle_t strobeQueue = getStrobeQueue();
    BaseType_t success;
    TickType_t prevWake;
    StrobeTaskCommand command;
    StrobeLED strobeInfo[MAX_STROBE_LEDS];
    int strobeInfoLen = 0;

    /* verify arguments */
    if (strobeQueue == NULL)
    {
        ESP_LOGE(TAG, "strobeTask was not created by createStrobeTask function!");
        throwFatalError();
    }

    while (true)
    {
        /* wait for and handle commands */
        if (strobeInfoLen == 0)
        {
            /* not currently strobing anything; block on the queue. Blocking on 
            the mutex is not possible here, so there special handling here of 
            when the queue is paused. */
            do {
                success = xQueueReceive(strobeQueue, &command, INT_MAX);
                receiveCommand(strobeInfo, &strobeInfoLen, &command);
            } while (success != pdTRUE);

            /* check if the queue is currently paused. If so, wait, then
            take the rest of the information from the queue */
            success = xSemaphoreTake(strobeQueueMutex, portMAX_DELAY);
            if (success != pdTRUE) throwFatalError();

            /* momentarily set queue processed event bit. If the waiting
            task misses this event, they can catch it in the next period. */
            (void) xEventGroupSetBits(strobeEvents, STROBE_QUEUE_PROCESSED_EVENT_BIT);
            (void) xEventGroupClearBits(strobeEvents, STROBE_QUEUE_PROCESSED_EVENT_BIT);

            success = xSemaphoreGive(strobeQueueMutex);
            if (success != pdTRUE) throwFatalError();
            /* now queue is resumed */

            prevWake = xTaskGetTickCount(); // start counting deadlines
        } else 
        {
            /* currently strobing things; check for a paused queue */
            success = xSemaphoreTake(strobeQueueMutex, 0);
            if (success)
            {
                /* queue is not paused, check it */
                do {
                    success = xQueueReceive(strobeQueue, &command, 0);
                    if (success) // item received from queue
                    {
                        receiveCommand(strobeInfo, &strobeInfoLen, &command);
                    }
                    
                } while (success != pdFALSE);

                /* momentarily set queue processed event bit. If the waiting
                task misses this event, they can catch it in the next period. */
                (void) xEventGroupSetBits(strobeEvents, STROBE_QUEUE_PROCESSED_EVENT_BIT);
                (void) xEventGroupClearBits(strobeEvents, STROBE_QUEUE_PROCESSED_EVENT_BIT);

                /* release mutex */
                success = xSemaphoreGive(strobeQueueMutex);
                if (success != pdTRUE) throwFatalError();
            }
        }

        /* strobe LEDs */
        strobeLEDs(strobeInfo, strobeInfoLen);
        success = xTaskDelayUntil(&prevWake, pdMS_TO_TICKS(STROBE_PERIOD));
        if (!success)
        {
            /* task was not delayed, ie. missed strobe deadline */
            ESP_LOGW(TAG, "Missed strobe deadline!");
        }
    }
    ESP_LOGE(TAG, "strobe task is exiting!");
    throwFatalError(); // this task should never exit
}

/**
 * @brief Finds the index of the target LED in strobeInfo.
 * 
 * @param[in] strobeInfo An array holding information about LEDs currently 
 *        being strobed.
 * @param[in] strobeInfoLen The number of elements in strobeInfo.
 * 
 * @returns The index of the target LED if successful, otherwise -1 if the
 *          target LED is not found.
 */
static int findStrobeLED(const StrobeLED strobeInfo[], 
                         int strobeInfoLen,
                         uint16_t targetLED)
{
    /* input guards */
    if (strobeInfo == NULL) return -1;

    /* find target */
    for (int ndx = 0; ndx < strobeInfoLen; ndx++)
    {
        if (strobeInfo[ndx].ledNum == targetLED) return ndx;
    }
    return -1;
}

/**
 * @brief Interprets the command and modifies strobeInfo accordingly.
 * 
 * @note Throws application errors if an error occurs.
 * 
 * @param[out] strobeInfo An array holding information about LEDs currently
 *        being strobed.
 * @param[out] strobeInfoLen A pointer to the number of elements in strobeInfo, 
 *        which will be modified to reflect changes in strobeInfo.
 * @param[in] command The command to interpret.
 * @param[in] errRes Error resources for thowing application errors.
 */
static void receiveCommand(StrobeLED strobeInfo[], 
                           int *strobeInfoLen, 
                           const StrobeTaskCommand *command)
{
    int ndx;

    /* input guards */
    if (strobeInfo == NULL) throwFatalError();
    if (strobeInfoLen == NULL) throwFatalError();
    if (command == NULL) throwFatalError();
    if (command->caller == NULL) throwFatalError();
    if (*strobeInfoLen == MAX_STROBE_LEDS && command->registerLED) throwFatalError();

    /* interpret command and modify strobeInfo */
    if (command->ledNum == UINT16_MAX) // indicates unregister all command
    {
        /* unregister all command */
        for (ndx = 0; ndx < *strobeInfoLen; ndx++)
        {
            if (strobeInfo[ndx].caller != command->caller) continue;
            
            ESP_LOGI(TAG, "Unregistering LED %d", strobeInfo[ndx].ledNum);
            strobeInfo[ndx] = strobeInfo[*strobeInfoLen - 1];
            *strobeInfoLen -= 1;
            ndx--; // need to check this index again bc new led is placed here
        }
        return;
    }

    ndx = findStrobeLED(strobeInfo, *strobeInfoLen, command->ledNum);
    if (command->registerLED)
    {
        /* register command */
        if (ndx != -1)
        {
            ESP_LOGW(TAG, "Ignoring attempt to register strobing for LED: %d", command->ledNum);
            ESP_LOGW(TAG, "Reason: LED is already being strobed");
            return;
        }
        
        ESP_LOGI(TAG, "Registering LED %d", strobeInfo[ndx].ledNum);
        strobeInfo[*strobeInfoLen].caller = command->caller;
        strobeInfo[*strobeInfoLen].ledNum = command->ledNum;
        strobeInfo[*strobeInfoLen].maxScale = command->maxScale;
        strobeInfo[*strobeInfoLen].minScale = command->minScale;
        strobeInfo[*strobeInfoLen].stepSizeHigh = command->stepSizeHigh;
        strobeInfo[*strobeInfoLen].stepSizeLow = command->stepSizeLow;
        strobeInfo[*strobeInfoLen].stepCutoff = command->stepCutoff;
        if (command->initScale >= command->maxScale)
        {
            strobeInfo[*strobeInfoLen].currScale = command->maxScale;
            strobeInfo[*strobeInfoLen].scalingUp = false;
        } else if (command->initScale <= command->minScale)
        {
            strobeInfo[*strobeInfoLen].currScale = command->minScale;
            strobeInfo[*strobeInfoLen].scalingUp = true;
        } else
        {
            strobeInfo[*strobeInfoLen].currScale = command->initScale;
            strobeInfo[*strobeInfoLen].scalingUp = command->initStrobeUp;
        }
        (*strobeInfoLen)++;
    } else
    {
        /* unregister command */
        if (ndx == -1)
        {
            ESP_LOGW(TAG, "Ignoring attempt to unregister strobing for LED: %d", command->ledNum);
            ESP_LOGW(TAG, "Reason: LED is not currently being strobed");
            return;
        }

        ESP_LOGI(TAG, "Unregistering LED %d", strobeInfo[ndx].ledNum);
        strobeInfo[ndx] = strobeInfo[*strobeInfoLen - 1]; // strobeInfoLen > 0 bc ndx != -1
        *strobeInfoLen -= 1;
    }
}

/**
 * @brief Handles strobing LEDs by ticking the scaling of each LED.
 * 
 * @note Throws application errors if an error occurs.
 * 
 * @param[in] strobeInfo An array holding information about LEDs currently
 *        being strobed.
 * @param[in] strobeInfoLen The length of strobeInfo.
 * @param[in] errRes Error resources for throwing application errors.
 */
static void strobeLEDs(StrobeLED strobeInfo[], const int strobeInfoLen)
{
    esp_err_t err;
    int ndx;

    /* input guards */
    if (strobeInfo == NULL) throwFatalError();

    /* strobe LEDs */
    for (ndx = 0; ndx < strobeInfoLen; ndx++)
    {
        int strobeStep;
        /* determine correct step size */
        if (strobeInfo[ndx].currScale > strobeInfo[ndx].stepCutoff)
        {
            strobeStep = strobeInfo[ndx].stepSizeHigh;
        } else if (strobeInfo[ndx].currScale < strobeInfo[ndx].stepCutoff) 
        {
            strobeStep = strobeInfo[ndx].stepSizeLow;
        } else
        {
            /* currScale == stepCutoff, choose step depending on direction */
            if (strobeInfo[ndx].scalingUp)
            {
                strobeStep = strobeInfo[ndx].stepSizeHigh;
            } else
            {
                strobeStep = strobeInfo[ndx].stepSizeLow;
            }
        }
        /* calculate new scaling value */
        if (strobeInfo[ndx].scalingUp)
        {
            if (strobeInfo[ndx].currScale + strobeStep < strobeInfo[ndx].currScale ||
                strobeInfo[ndx].currScale + strobeStep >= strobeInfo[ndx].maxScale)
            {
                /* maxval or overflow would occur, cap at maxval and change dir */
                strobeInfo[ndx].currScale = strobeInfo[ndx].maxScale;
                strobeInfo[ndx].scalingUp = false;
            } else
            {
                strobeInfo[ndx].currScale += strobeStep;
            }
        } else 
        {
            if (strobeInfo[ndx].currScale - strobeStep > strobeInfo[ndx].currScale ||
                strobeInfo[ndx].currScale - strobeStep <= strobeInfo[ndx].minScale)
            {
                /* minval or underflow would occur, cap at minval and change dir */
                strobeInfo[ndx].currScale = strobeInfo[ndx].minScale;
                strobeInfo[ndx].scalingUp = true;
            } else
            {
                strobeInfo[ndx].currScale -= strobeStep;
            }
        }

        /* send I2C transaction to scale value */
        for (int i = 0; i < RETRY_I2C_NUM; i++)
        {
            uint8_t scaleVal = strobeInfo[ndx].currScale;
            err = matSetScaling(strobeInfo[ndx].ledNum, scaleVal, scaleVal, scaleVal);
            if (err == ESP_OK) break;
        }
    }
}

#endif /* CONFIG_SUPPORT_STROBING */