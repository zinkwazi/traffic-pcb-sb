/**
 * tasks.h
 * 
 * This file contains task functions
 * that allow the application to be efficient.
 */

#ifndef WORKER_H_
#define WORKER_H_

#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "utilities.h"
#include "dots_commands.h"
#include "pinout.h"
#include "tasks.h"
#include "main_types.h"
#include "error.h"

#define TAG "tasks"

#define RETRY_CREATE_HTTP_HANDLE_TICKS 500
#define CHECK_ERROR_PERIOD_TICKS 500

/**
 * @brief Describes the type of command that the worker task will handle.
 */
enum WorkerCommandType {
    /* refresh the dots moving from south to north */
    REFRESH_NORTH,
    /* refresh the dots moving from north to south */
    REFRESH_SOUTH,
    /* clear the dots moving from south to north */
    CLEAR_NORTH,
    /* clear the dots moving from north to south */
    CLEAR_SOUTH,
    /* clear the dots by resetting the dot matrices, doubles as matrix initialization */
    QUICK_CLEAR,
};

typedef enum WorkerCommandType WorkerCommandType;

/**
 * @brief A command for the dot worker task, eventually to hold animation info.
 * */
struct WorkerCommand {
    WorkerCommandType type;
};

typedef struct WorkerCommand WorkerCommand;

/**
 * @brief Stores references to objects necessary for the worker task.
 * 
 * @note The dot worker task, implemented by vWorkerTask, does its work 
 * within the context of these resources and is created by createWorkerTask.
 */
struct WorkerTaskResources {
  QueueHandle_t dotQueue; /*!< A handle to a queue that holds struct DotCommand 
                               objects. This task retrieves commands from this 
                               queue and performs work to fulfill them. */
  QueueHandle_t I2CQueue; /*!< A handle to a queue that holds struct I2CCommand 
                               objects. This task issues commands to this queue 
                               to be handled by the I2C gatekeeper, implemented 
                               by vI2CGatekeeperTask. */
  ErrorResources *errRes; /*!< Holds global error handling resources. */
};

typedef struct WorkerTaskResources WorkerTaskResources;

/**
 * @brief Initializes the worker task, which is implemented by vWorkerTask.
 * 
 * @param handle A pointer to a handle which will refer to the created task
 *               if successful.
 * @param dotQueue A handle to a queue that holds struct DotCommand 
 *                 objects. This task retrieves commands from this 
 *                 queue and performs work to fulfill them.
 * @param I2CQueue A handle to a queue that holds struct I2CCommand 
 *                 objects. This task issues commands to this queue 
 *                 to be handled by the I2C gatekeeper, implemented 
 *                 by vI2CGatekeeperTask.
 * @param errRes A pointer to global error handling resources.
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t createWorkerTask(TaskHandle_t *handle, QueueHandle_t dotQueue, QueueHandle_t I2CQueue, ErrorResources *errRes);

/**
 * @brief Implements the worker task, which is responsible for handling
 *        commands of type WorkerCommand sent from the main task.
 * 
 * @note The worker task receives commands from the main task. It is the task 
 *       that does the most 'business logic' of the application. It relieves 
 *       the main task of these duties so that it can quickly respond to user 
 *       input.
 * 
 * @param pvParameters A pointer to a WorkerTaskResources object which
 *                     should remain valid through the lifetime of the task.
 */
void vWorkerTask(void *pvParameters);

/**
 * @brief Initializes the over-the-air (OTA) task, which is implemented by
 *        vOTATask.
 * 
 * @note This function creates shallow copies of parameters that will be 
 *       provided to the task in static memory. It assumes that only one of 
 *       this type of task will be created; any additional tasks will have 
 *       pointers to the same location in static memory.
 * 
 * @param handle A pointer to a handle which will refer to the created task
 *               if successful.
 * @param errorResources A pointer to an ErrorResources object. A deep copy
 *                       of the object will be created in static memory.
 *                       
 * @returns ESP_OK if the task was created successfully, otherwise ESP_FAIL.
 */
esp_err_t createOTATask(TaskHandle_t *handle, const ErrorResources *errorResources);

/**
 * @brief Implements the over-the-air (OTA) task, which is responsible for
 *        handling user requests to update to the latest version of firmware.
 * 
 * @note To avoid runtime errors, the OTA task should only be created by the
 *       createOTATask function.
 * 
 * @param pvParameters A pointer to an ErrorResources object which should
 *                     remain valid through the lifetime of the task.
 */
void vOTATask(void* pvParameters);

#endif /* WORKER_H_ */