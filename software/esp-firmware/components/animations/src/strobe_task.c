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
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"

#include "app_errors.h"
#include "led_matrix.h"
#include "led_registers.h"

#define TAG "strobe_task"

/* The size in commands of the strobe command queue */
#define STROBE_QUEUE_SIZE (20)

/* The maximum number of LEDs the strobe task can handle simultaneously */
#define MAX_STROBE_LEDS (MAX_NUM_LEDS_REG)

/* The strobe period in milliseconds of strobing steps. Increasing this
   and increasing strobe steps will increase quantization at the same
   literal strobe period, while potentially fixing missed deadlines. */
#define STROBE_PERIOD (100)

/* The percentage of the way from MIN_SCALE_VAL to MAX_SCALE_VAL
   that HIGH strobe steps will take effect instead of LOW strobe steps*/
#define SCALE_CUTOFF_FRAC (1.0 / 4.0)

/* The scaling step size change per period */
#define STROBE_STEP_UP_HIGH (30)
#define STROBE_STEP_DOWN_HIGH (30)

#define STROBE_STEP_UP_LOW (1)
#define STROBE_STEP_DOWN_LOW (1)


/* The number of times to retry I2C transactions for scaling */
#define RETRY_I2C_NUM (5)


/* The strobe task command queue of size STROBE_QUEUE_SIZE, initialized by 
   createStrobeTask. Tasks will register and unregister led strobing by adding 
   commands to this queue. The queue should be accessed through getStrobeQueue
   in order to reduce the risk of accidentally overwriting this handle. */
static QueueHandle_t strobeQueue = NULL; // holds StrobeTaskCommand

/**
 * @brief Returns a copy of the strobeQueue handle. The strobeQueue holds
 *        StrobeTaskCommand objects.
 */
QueueHandle_t getStrobeQueue(void)
{
    return strobeQueue; // make sure that others cannot modify the base handle
}

static void vStrobeTask(void *pvParameters);
static int findStrobeLED(const StrobeLED strobeInfo[], int strobeInfoLen, uint16_t targetLED);
static void receiveCommand(StrobeLED strobeInfo[], int *strobeInfoLen, const StrobeTaskCommand *command, ErrorResources *errRes);
static void strobeLEDs(StrobeLED strobeInfo[], const int strobeInfoLen, ErrorResources *errRes);

/**
 * @brief Initializes the over-the-air (OTA) task, which is implemented by
 *        vOTATask.
 * 
 * @note This function creates shallow copies of parameters that will be 
 *       provided to the task in static memory. It assumes that only one of 
 *       this type of task will be created; any additional tasks will have 
 *       pointers to the same location in static memory.
 * 
 * @param[out] handle A pointer to a handle which will refer to the created task
 *        if successful. Optional--can be set to NULL.       
 * @param[in] errorResources A pointer to an ErrorResources object. A deep copy
 *        of the object will be created in static memory.
 *                       
 * @returns ESP_OK if the task was created successfully.
 *          ESP_ERR_INVALID_ARG if invalid argument.
 *          ESP_ERR_NO_MEM if not enough freeRTOS dynamic memory was allocated.
 *          ESP_FAIL if createStrobeTask was already called. In this case,
 *          recovery is not possible and the device must be restarted.
 */
esp_err_t createStrobeTask(TaskHandle_t *handle,
                           ErrorResources *errorResources)
{
    static StrobeTaskResources resources;
    BaseType_t success;

    /* input guards */
    if (errorResources == NULL) return ESP_ERR_INVALID_ARG;
    if (errorResources->errMutex == NULL) return ESP_ERR_INVALID_ARG;

    /* copy arguments */
    resources.errRes = errorResources;

    /* initialize command queue */
    if (strobeQueue != NULL) return ESP_FAIL; // already initialized
    strobeQueue = xQueueCreate(STROBE_QUEUE_SIZE, sizeof(StrobeTaskCommand));
    if (strobeQueue == NULL) return ESP_ERR_NO_MEM;

    /* create OTA task */
    success = xTaskCreate(vStrobeTask, "StrobeTask", CONFIG_STROBE_STACK,
                            &resources, CONFIG_STROBE_PRIO, handle);
    return (success == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
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
    ErrorResources *const errRes = ((StrobeTaskResources *) pvParameters)->errRes;
    const QueueHandle_t strobeQueue = getStrobeQueue();
    BaseType_t success;
    TickType_t prevWake;
    StrobeTaskCommand command;
    StrobeLED strobeInfo[MAX_STROBE_LEDS];
    int strobeInfoLen = 0;

    /* verify arguments */
    if (errRes == NULL || errRes->errMutex == NULL)
    {
        ESP_LOGE(TAG, "strobeTask provided NULL errRes!");
        throwFatalError(NULL, false);
    }
    if (strobeQueue == NULL)
    {
        ESP_LOGE(TAG, "strobeTask was not created by createStrobeTask function!");
        throwFatalError(errRes, false);
    }
    
    
    while (true)
    {
        /* wait for and handle commands */
        if (strobeInfoLen == 0)
        {
            do {
                success = xQueueReceive(strobeQueue, &command, INT_MAX);
            } while (success != pdPASS);
            prevWake = xTaskGetTickCount(); // start counting deadlines
        } else 
        {
            success = xQueueReceive(strobeQueue, &command, 0);
        }

        if (success == pdPASS)
        {
            /* a command was received from the queue */
            receiveCommand(strobeInfo, &strobeInfoLen, &command, errRes);
            continue; // receive commands until queue is empty
        }

        /* strobe LEDs */
        strobeLEDs(strobeInfo, strobeInfoLen, errRes);
        success = xTaskDelayUntil(&prevWake, pdMS_TO_TICKS(STROBE_PERIOD));
        if (!success)
        {
            /* task was not delayed, ie. missed strobe deadline */
            ESP_LOGW(TAG, "Missed strobe deadline!");
        }
    }

    ESP_LOGE(TAG, "strobe task is exiting!");
    throwFatalError(errRes, false); // this task should never exit
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
                           const StrobeTaskCommand *command, 
                           ErrorResources *errRes)
{
    int ndx;

    /* input guards */
    if (strobeInfo == NULL) throwFatalError(errRes, false);
    if (strobeInfoLen == NULL) throwFatalError(errRes, false);
    if (command == NULL) throwFatalError(errRes, false);
    if (command->caller == NULL) throwFatalError(errRes, false);
    if (*strobeInfoLen == MAX_STROBE_LEDS && command->registerLED) throwFatalError(errRes, false);

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
static void strobeLEDs(StrobeLED strobeInfo[], const int strobeInfoLen, ErrorResources *errRes)
{
    esp_err_t err;
    int ndx;

    /* input guards */
    if (errRes == NULL) throwFatalError(NULL, false);
    if (strobeInfo == NULL) throwFatalError(errRes, false);

    /* strobe LEDs */
    for (ndx = 0; ndx < strobeInfoLen; ndx++)
    {
        int strobeStep;
        /* determine correct step size */
        if (strobeInfo[ndx].currScale > ((int) ((strobeInfo[ndx].maxScale - strobeInfo[ndx].minScale) * SCALE_CUTOFF_FRAC)) + strobeInfo[ndx].minScale)
        {
            if (strobeInfo[ndx].scalingUp)
            {
                strobeStep = STROBE_STEP_UP_HIGH;
            } else 
            {
                strobeStep = STROBE_STEP_DOWN_HIGH;
            }
        } else 
        {
            if (strobeInfo[ndx].scalingUp)
            {
                strobeStep = STROBE_STEP_UP_LOW;
            } else
            {
                strobeStep = STROBE_STEP_DOWN_LOW;
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