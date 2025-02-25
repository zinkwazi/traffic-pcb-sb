/**
 * dots_commands.c
 *
 * This file contains wrapper functions that
 * place an element on the I2C gatekeeper queue
 * to interact with LED matrices.
 */

#include "dots_commands.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c_master.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"

#include "dots_matrix.h"
#include "pinout.h"

/* A struct for placing parameters on the I2C command queue */
struct SetColorParams
{
    uint16_t ledNum;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

/* A struct for placing parameters on the I2C command queue */
struct SetScalingParams
{
    uint16_t ledNum;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

/**
 * This function maps the I2CCommandFunc enum to actual functions
 * and executes them, performing error callbacks when necessary.
 */
void executeI2CCommand(PageState *state, MatrixHandles *matrices, I2CCommand *command)
{
    esp_err_t err = ESP_OK;
    ESP_LOGD(TAG, "executing I2C command...");
    switch (command->func)
    {
    case SET_OPERATING_MODE:
        ESP_LOGD(TAG, "setting operating mode");
        err = dSetOperatingMode(state, *matrices, *((enum Operation *)command->params));
        break;
    case SET_OPEN_SHORT_DETECTION:
        ESP_LOGD(TAG, "changing open/short detection");
        err = dSetOpenShortDetection(state, *matrices, *((enum ShortDetectionEnable *)command->params));
        break;
    case SET_LOGIC_LEVEL:
        ESP_LOGD(TAG, "changing logic level");
        err = dSetLogicLevel(state, *matrices, *((enum LogicLevel *)command->params));
        break;
    case SET_SWX_SETTING:
        ESP_LOGD(TAG, "changing SWx setting");
        err = dSetSWxSetting(state, *matrices, *((enum SWXSetting *)command->params));
        break;
    case SET_GLOBAL_CURRENT_CONTROL:
        ESP_LOGD(TAG, "changing global current control setting");
        err = dSetGlobalCurrentControl(state, *matrices, *((uint8_t *)command->params));
        break;
    case SET_RESISTOR_PULLUP:
        ESP_LOGD(TAG, "changing resistor pullup setting");
        err = dSetResistorPullupSetting(state, *matrices, *((enum ResistorSetting *)command->params));
        break;
    case SET_RESISTOR_PULLDOWN:
        ESP_LOGD(TAG, "changing resistor pulldown setting");
        err = dSetResistorPulldownSetting(state, *matrices, *((enum ResistorSetting *)command->params));
        break;
    case SET_PWM_FREQUENCY:
        ESP_LOGD(TAG, "changing PWM frequency");
        err = dSetPWMFrequency(state, *matrices, *((enum PWMFrequency *)command->params));
        break;
    case RESET:
        ESP_LOGD(TAG, "resetting matrices");
        err = dReset(state, *matrices);
        break;
    case SET_COLOR:
        ESP_LOGD(TAG, "changing dot color");
        struct SetColorParams *setColorParams = (struct SetColorParams *)command->params;
        err = dSetColor(state, *matrices,
                        setColorParams->ledNum,
                        setColorParams->red,
                        setColorParams->green,
                        setColorParams->blue);
        break;
    case SET_SCALING:
        ESP_LOGD(TAG, "changing dot scaling");
        struct SetScalingParams *setScalingParams = (struct SetScalingParams *)command->params;
        err = dSetScaling(state, *matrices,
                          setScalingParams->ledNum,
                          setScalingParams->red,
                          setScalingParams->green,
                          setScalingParams->blue);
        break;
#if CONFIG_DISABLE_TESTING_FEATURES == false
    case RELEASE_BUS:
        err = dReleaseBus(matrices);
        break;
    case REAQUIRE_BUS:
        err = dInitializeBus(state, matrices, I2C_PORT, SDA_PIN, SCL_PIN);
        break;
    case NOTIFY_OK_VAL:
        err = ESP_OK;
        break;
    case NOTIFY_ERR_VAL:
        err = ESP_FAIL;
        break;
#endif /* CONFIG_DISABLE_TESTING_FEATURES == false */
    default:
        ESP_LOGW(TAG, "I2C gatekeeper recieved an invalid command");
        return;
    }
    if (err != ESP_OK && command->notifyTask != NULL)
    {
        xTaskNotify(command->notifyTask, DOTS_ERR_VAL, eSetValueWithOverwrite);
    }
    else if (err == ESP_OK && command->notifyTask != NULL)
    {
        xTaskNotify(command->notifyTask, DOTS_OK_VAL, eSetValueWithOverwrite);
    }
}

/**
 * @brief Initializes the I2C gatekeeper task, which is implemented by
 *        vI2CGatekeeperTask.
 *
 * @note The gatekeeper is intended to be the only task that interacts with the
 *       I2C peripheral in order to keep dot matrices in known states.
 *
 * @param handle A pointer to a handle which will refer to the created task
 *               if successful.
 * @param I2CQueue A handle to a queue that holds struct I2CCommand
 *                 objects. This task retrieves commands from this queue and
 *                 performs I2C transactions to fulfill them.
 * @param port The port of the I2C bus lines.
 * @param sdaPin The pin of the SDA I2C line on the I2C port.
 * @param sclPin The pin of the SCL I2C line on the I2C port.
 *
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t createI2CGatekeeperTask(TaskHandle_t *handle, QueueHandle_t I2CQueue)
{
    static I2CGatekeeperTaskParams taskResources;
    BaseType_t success;
    /* input guards */
    if (I2CQueue == NULL)
    {
        return ESP_FAIL;
    }
    /* package parameters */
    taskResources.I2CQueue = I2CQueue;
    /* create task */
    success = xTaskCreate(vI2CGatekeeperTask, "I2CGatekeeper", CONFIG_I2C_GATEKEEPER_STACK,
                          &taskResources, CONFIG_I2C_GATEKEEPER_PRIO, handle);
    return (success) ? ESP_OK : ESP_FAIL;
}

/**
 * This task manages interaction with the I2C peripheral,
 * which should be interacted with only through the functions
 * below. These functions abstract queueing interaction with
 * the d matrices.
 */
void vI2CGatekeeperTask(void *pvParameters)
{
    I2CGatekeeperTaskParams *params = (I2CGatekeeperTaskParams *)pvParameters;
    I2CCommand command;
    PageState state;
    MatrixHandles matrices;
    esp_err_t err;
    /* One time setup */
    err = dInitializeBus(&state, &matrices, I2C_PORT, SDA_PIN, SCL_PIN);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialize I2C bus");
    }
    err = ESP_FAIL;
    while (err != ESP_OK)
    {
        err = dAssertConnected(&state, matrices);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "could not find i2c matrices... retrying...");
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    /* Wait for commands and execute them forever */
    for (;;)
    { // This task should never end
        if (pdPASS != xQueueReceive(params->I2CQueue, &command, INT_MAX))
        {
            ESP_LOGD(TAG, "I2C Gatekeeper timed out while waiting for command on queue");
            continue;
        }
        executeI2CCommand(&state, &matrices, &command);
        if (command.params != NULL)
        {
            free(command.params);
            command.params = NULL;
        }
    }
    ESP_LOGE(TAG, "I2C gatekeeper task is exiting! This should be impossible!");
    vTaskDelete(NULL); // exit safely (should never happen)
}

/**
 * @brief Adds a command to the I2CQueue and optionally blocks on a task notification
 *        expected to come from the I2CGatekeeper when it has finished the request.
 *
 * This task blocks on a task notification when notifyTask is not NULL and blocking
 * is true, ie. DOTS_BLOCKING. When blocking is false, ie. DOTS_ASYNC, a task
 * notification is still sent if notifyTask is not NULL. The caller must take care
 * that task notifications from the gatekeeper are handled because the gatekeeper
 * overrides the return value from any unhandled task notifications when it has
 * finished with another command. It is recommended to either use DOTS_BLOCKING
 * to ensure that all notifications are retrieved, or entirely disable notifications
 * by setting notifyTask to NULL.
 *
 * @param[in] queue The I2CQueue, which holds I2CCommand objects that the
 *        gatekeeper services.
 * @param[in] func The command to give the I2CGatekeeper.
 * @param[in] params A pointer to dynamically allocated memory that the gatekeeper
 *        becomes the owner of. This memory should correspond to the parameters
 *        required by the function the gatekeeper will use to service the command.
 * @param[in] notifyTask The handle of the task that will receive a task notification
 *        when the gatekeeper completes the command. If NULL, no notification will be sent.
 * @param[in] blocking Whether this task should wait for a task notification within
 *        this function after adding a command to the queue or not. Will not block
 *        if notifyTask is NULL.
 *
 * @returns ESP_OK if non-blocking and successfully added a command to the queue.
 *          ESP_OK if blocking and received DOTS_OK_VAL task notification.
 *          DOTS_ERR_VAL if blocking and received non-DOTS_OK_VAL task notification.
 *          ESP_FAIL otherwise (caller should free params and not expect a task notification).
 */
esp_err_t addCommandToI2CQueue(QueueHandle_t queue, enum I2CCommandFunc func, void *params, TaskHandle_t notifyTask, bool blocking)
{
    I2CCommand command = {
        // queueing is by copy, not reference
        .func = func,
        .params = params,
        .notifyTask = notifyTask,
    };
    /* input guards */
    if (queue == NULL || func >= I2CCOMMANDFUNC_MAX)
    {
        return ESP_FAIL;
    }
    /* add command to queue */
    while (xQueueSendToBack(queue, &command, INT_MAX) != pdTRUE)
    {
        ESP_LOGE(TAG, "failed to add command to queue, retrying...");
    }
    if (notifyTask != NULL && blocking)
    {
        uint32_t returnValue = 0;
        while (returnValue == 0)
        {
            returnValue = ulTaskNotifyTake(pdTRUE, INT_MAX);
        }
        switch (returnValue)
        {
        case DOTS_OK_VAL:
            return ESP_OK;
        case DOTS_ERR_VAL:
            return DOTS_ERR_VAL;
        default:
            ESP_LOGE(TAG, "received unknown notification value from gatekeeper: %lu", returnValue);
            return DOTS_ERR_VAL;
        }
    }
    return ESP_OK;
}

/**
 * @brief Sends a command to the I2CGatekeeper to put each of the matrices
 *        into the provided operation mode.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] setting The operation mode to put each of the matrices in.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command. 
 */
esp_err_t dotsSetOperatingMode(QueueHandle_t queue, enum Operation setting, bool notify, bool blocking)
{
    esp_err_t err;
    /* input guards */
    if (queue == NULL || setting >= MATRIX_OPERATION_MAX)
    {
        return ESP_FAIL;
    }
    /* copy params to heap */
    enum Operation *heapSetting = malloc(sizeof(enum Operation)); // owner becomes I2CGatekeeper
    if (heapSetting == NULL)
    {
        return ESP_FAIL;
    }
    *heapSetting = setting;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_OPERATING_MODE, (void *) heapSetting, notifyTask, blocking);
    if (err == ESP_FAIL)
    {
        free(heapSetting);
        return err;
    }
    return err;
}

/**
 * @brief Sends a command to the I2CGatekeeper to put each of the matrices into
 *        the provided open/short detection mode.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] setting The detection mode to put each of the matrices in.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command. 
 */
esp_err_t dotsSetOpenShortDetection(QueueHandle_t queue, enum ShortDetectionEnable setting, bool notify, bool blocking)
{
    esp_err_t err;
    /* input gaurds */
    if (queue == NULL || setting >= MATRIX_SHORT_DETECTION_EN_MAX)
    {
        return ESP_FAIL;
    }
    /* copy params to heap */
    enum ShortDetectionEnable *heapSetting = malloc(sizeof(enum ShortDetectionEnable)); // owner becomes I2CGatekeeper
    if (heapSetting == NULL)
    {
        return ESP_FAIL;
    }
    *heapSetting = setting;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_OPEN_SHORT_DETECTION, (void *) heapSetting, notifyTask, blocking);
    if (err == ESP_FAIL)
    {
        free(heapSetting);
        return err;
    }
    return err;
}

/**
 * @brief Sends a command to the I2CGatekeeper to put each of the matrices into
 *        the provided logic level.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] setting The logic level to put each of the matrices in.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsSetLogicLevel(QueueHandle_t queue, enum LogicLevel setting, bool notify, bool blocking)
{
    esp_err_t err;
    /* input guards */
    if (queue == NULL || setting >= MATRIX_LOGIC_LEVEL_MAX)
    {
        return ESP_FAIL;
    }
    /* copy params to heap */
    enum LogicLevel *heapSetting = malloc(sizeof(enum LogicLevel)); // owner becomes I2CGatekeeper
    if (heapSetting == NULL)
    {
        return ESP_FAIL;
    }
    *heapSetting = setting;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_LOGIC_LEVEL, (void *) heapSetting, notifyTask, blocking);
    if (err == ESP_FAIL)
    {
        free(heapSetting);
        return err;
    }
    return err;
}

/**
 * @brief Sends a command to the I2CGatekeeper to put each of the matrices into
 *        the provided SWx setting.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] setting The SWx setting to put each of the matrices in.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsSetSWxSetting(QueueHandle_t queue, enum SWXSetting setting, bool notify, bool blocking)
{
    esp_err_t err;
    /* input guards */
    if (queue == NULL || setting >= MATRIX_SWXSETTING_MAX)
    {
        return ESP_FAIL;
    }
    /* copy params to heap */
    enum SWXSetting *heapSetting = malloc(sizeof(enum SWXSetting)); // owner becomes I2CGatekeeper
    if (heapSetting == NULL)
    {
        return ESP_FAIL;
    }
    *heapSetting = setting;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_SWX_SETTING, (void *) heapSetting, notifyTask, blocking);
    if (err == ESP_FAIL)
    {
        free(heapSetting);
        return err;
    }
    return err;
}

/**
 * @brief Sends a command to the I2CGatekeeper to set the global current control
 *        level of each of the matrices.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] setting The global current control to set for each matrix.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsSetGlobalCurrentControl(QueueHandle_t queue, uint8_t value, bool notify, bool blocking)
{
    esp_err_t err;
    /* input gaurds */
    if (queue == NULL) {
        return ESP_FAIL;
    }
    /* copy params to heap */
    uint8_t *heapValue = malloc(sizeof(uint8_t)); // owner becomes I2CGatekeeper
    if (heapValue == NULL)
    {
        return ESP_FAIL;
    }
    *heapValue = value;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_GLOBAL_CURRENT_CONTROL, (void *) heapValue, notifyTask, blocking);
    if (err == ESP_FAIL) {
        free(heapValue);
        return err;
    }
    return err;
}

/**
 * @brief Sends a command to the I2CGatekeeper to set the resistor pullup
 *        value of each matrix to the provided value.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] setting The value to set each matrix to.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsSetResistorPullupSetting(QueueHandle_t queue, enum ResistorSetting setting, bool notify, bool blocking)
{
    esp_err_t err;
    /* input guards */
    if (queue == NULL || setting >= MATRIX_RESISTORSETTING_MAX) {
        return ESP_FAIL;
    }
    /* copy params to heap */
    enum ResistorSetting *heapSetting = malloc(sizeof(enum ResistorSetting)); // owner becomes I2CGatekeeper
    if (heapSetting == NULL)
    {
        return ESP_FAIL;
    }
    *heapSetting = setting;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_RESISTOR_PULLUP, (void *) heapSetting, notifyTask, blocking);
    if (err == ESP_FAIL) {
        free(heapSetting);
        return err;
    }
    return err;
}

/**
 * @brief Sends a command to the I2CGatekeeper to set the resistor pulldown
 *        value of each matrix to the provided value.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] setting The value to set each matrix to.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsSetResistorPulldownSetting(QueueHandle_t queue, enum ResistorSetting setting, bool notify, bool blocking)
{
    esp_err_t err;
    /* input guards */
    if (queue == NULL || setting >= MATRIX_RESISTORSETTING_MAX) {
        return ESP_FAIL;
    }
    /* copy params to heap */
    enum ResistorSetting *heapSetting = malloc(sizeof(enum ResistorSetting)); // owner becomes I2CGatekeeper
    if (heapSetting == NULL)
    {
        return ESP_FAIL;
    }
    *heapSetting = setting;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_RESISTOR_PULLDOWN, (void *) heapSetting, notifyTask, blocking);
    if (err == ESP_FAIL) {
        free(heapSetting);
        return err;
    }
    return err;
}

/**
 * @brief Sends a command to the I2CGatekeeper to set the PWM frequency of each 
 *        matrix to the provided value.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] setting The frequency to set each matrix to.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsSetPWMFrequency(QueueHandle_t queue, enum PWMFrequency freq, bool notify, bool blocking)
{
    esp_err_t err;
    /* input guards */
    if (queue == NULL ||
        freq == MATRIX_PWMFREQ_INVALID_1 ||
        freq == MATRIX_PWMFREQ_INVALID_3 ||
        freq == MATRIX_PWMFREQ_INVALID_4 ||
        freq == MATRIX_PWMFREQ_INVALID_5 ||
        freq == MATRIX_PWMFREQ_INVALID_6 ||
        freq == MATRIX_PWMFREQ_INVALID_8 ||
        freq == MATRIX_PWMFREQ_INVALID_9 ||
        freq == MATRIX_PWMFREQ_INVALID_10 ||
        freq >= MATRIX_PWMFREQ_MAX) 
    {
        return ESP_FAIL;
    }
    /* copy params to heap */
    enum PWMFrequency *heapFreq = malloc(sizeof(enum PWMFrequency)); // owner becomes I2CGatekeeper
    if (heapFreq == NULL)
    {
        return ESP_FAIL;
    }
    *heapFreq = freq;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_PWM_FREQUENCY, (void *) heapFreq, notifyTask, blocking);
    if (err == ESP_FAIL) {
        free(heapFreq);
        return err;
    }
    return err;
}

/**
 * @brief Sends a command to the I2CGatekeeper to reset all matrix registers
 *        to default values.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsReset(QueueHandle_t queue, bool notify, bool blocking)
{
    /* input gaurds */
    if (queue == NULL) {
        return ESP_FAIL;
    }
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    return addCommandToI2CQueue(queue, RESET, NULL, notifyTask, blocking);
}

/**
 * @brief Sends a command to the I2CGatekeeper to set the color of the LED
 *        corresponding to Kicad hardware number ledNum. Internally, this
 *        changes the PWM duty of the LED in 256 steps.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] ledNum The Kicad hardware number corresponding to the target LED.
 * @param[in] red The PWM duty of the red diode in the LED.
 * @param[in] green The PWM duty of the green diode in the LED.
 * @param[in] blue The PWM duty of the blue diode in the LED.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsSetColor(QueueHandle_t queue, uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue, bool notify, bool blocking)
{
    esp_err_t err;
    /* input guards */
    if (queue == NULL) {
        return ESP_FAIL;
    }
    /* copy parameters to heap */
    struct SetColorParams *heapParams = malloc(sizeof(struct SetColorParams)); // owner becomes I2CGatekeeper
    if (heapParams == NULL)
    {
        return ESP_FAIL;
    }
    heapParams->ledNum = ledNum;
    heapParams->red = red;
    heapParams->green = green;
    heapParams->blue = blue;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_COLOR, (void *)heapParams, notifyTask, blocking);
    if (err == ESP_FAIL) {
        free(heapParams);
        return err;
    }
    return err;
}

/**
 * @brief Sends a command to the I2CGatekeeper to set the DC output current of
 *        the LED corresponding to Kicad hardware number ledNum. See pg. 13 of
 *        the datasheet for exact current calculations. This can be considered
 *        a dimming function.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] ledNum The Kicad hardware number corresponding to the target LED.
 * @param[in] red The scaling of the red diode in the LED.
 * @param[in] green The scaling of the green diode in the LED.
 * @param[in] blue The scaling of the blue diode in the LED.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsSetScaling(QueueHandle_t queue, uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue, bool notify, bool blocking)
{
    esp_err_t err;
    /* input guards */
    if (queue == NULL) {
        return ESP_FAIL;
    }
    /* copy parameters to heap */
    struct SetScalingParams *heapParams = malloc(sizeof(struct SetColorParams)); // owner becomes I2CGatekeeper
    if (heapParams == NULL)
    {
        return ESP_FAIL;
    }
    heapParams->ledNum = ledNum;
    heapParams->red = red;
    heapParams->green = green;
    heapParams->blue = blue;
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    err = addCommandToI2CQueue(queue, SET_SCALING, (void *) heapParams, notifyTask, blocking);
    if (err == ESP_FAIL) {
        free(heapParams);
        return err;
    }
    return err;
}

#if CONFIG_DISABLE_TESTING_FEATURES == false // this is inverted for the esp-idf vscode extension
/*******************************************/
/*            TESTING FEATURES             */
/*******************************************/

/**
 * @brief Sends a command to the I2CGatekeeper telling it to release resources
 *        it has taken for the I2C bus. This is sometimes necessary for tests
 *        to directly interact with the I2C bus to check results.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsReleaseBus(QueueHandle_t queue, bool notify, bool blocking)
{
    /* input guards */
    if (queue == NULL) {
        return ESP_FAIL;
    }
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    return addCommandToI2CQueue(queue, RELEASE_BUS, NULL, notifyTask, blocking);
}

/**
 * @brief Sends a command to the I2CGatekeeper telling it to reinitialize
 *        resources to take control of the I2C bus. This is sometimes necessary
 *        after direct interactino with the I2C bus after dotsReleaseBus.
 * 
 * @param[in] queue The I2CQueue from which the I2CGatekeeper receives commands.
 * @param[in] notify Either DOTS_NOTIFY or DOTS_SILENT. Whether the gatekeeper
 *        should send a task notification when it has completed the command.
 * @param[in] blocking Either DOTS_BLOCKING or DOTS_ASYNC. Whether the task
 *        should wait for a task notification from the gatekeeper after sending
 *        the command. Will not block if DOTS_SILENT is chosen.
 *
 * @returns ESP_OK if command was sent and completed successfully.
 *          DOTS_ERR_VAL if command was sent but not completed successfully. In
 *          this case, the configuration of each matrix may have changed, but
 *          not all of the matrices have changed.
 *          ESP_FAIL if an error occurred when sending the command.
 */
esp_err_t dotsReaquireBus(QueueHandle_t queue, bool notify, bool blocking)
{
    /* input guards */
    if (queue == NULL) {
        return ESP_FAIL;
    }
    /* send command */
    TaskHandle_t notifyTask = (notify) ? xTaskGetCurrentTaskHandle() : NULL;
    return addCommandToI2CQueue(queue, REAQUIRE_BUS, NULL, notifyTask, blocking);
}

#endif /* CONFIG_DISABLE_TESTING_FEATURES == false */