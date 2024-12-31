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

#define TAG "tasks"

#define RETRY_CREATE_HTTP_HANDLE_TICKS 500
#define CHECK_ERROR_PERIOD_TICKS 500

/** @brief The stack size allocated for the OTA task. */
#define OTA_TASK_STACK (ESP_TASK_MAIN_STACK)
/** @brief The task priority of the OTA task. */
#define OTA_TASK_PRIO (4)


/** @brief The task priority of the main task. */
#define MAIN_TASK_PRIO (3)


/** @brief The stack size allocated for the I2C gatekeeper task. */
#define I2C_GATEKEEPER_STACK (ESP_TASK_MAIN_STACK - 1400)
/** @brief The task priority of the I2C gatekeeper task. */
#define I2C_GATEKEEPER_PRIO (2)
/** @brief The queue size in elements of the I2C command queue. */
#define I2C_QUEUE_SIZE 20


/** @brief The stack size allocated for the dot worker task. */
#define DOTS_WORKER_STACK (ESP_TASK_MAIN_STACK + 1000)
/** @brief The task priority of the dot worker task. */
#define DOTS_WORKER_PRIO (1)
/** @brief The queue size of the dot command queue. */
#define DOTS_QUEUE_SIZE 1

enum DotCommandType {
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

typedef enum DotCommandType DotCommandType;

/* A command for the dot worker task, eventually to hold animation info */
struct DotCommand {
    DotCommandType type;
};

typedef struct DotCommand DotCommand;

/**
 * @brief Stores references to objects necessary for the worker task.
 * 
 * @note The dot worker task, implemented by vDotWorkerTask, does its work 
 * within the context of these resources and is created by createDotWorkerTask.
 */
struct DotWorkerTaskResources {
  QueueHandle_t dotQueue; /*!< A handle to a queue that holds struct DotCommand 
                               objects. This task retrieves commands from this 
                               queue and performs work to fulfill them. */
  QueueHandle_t I2CQueue; /*!< A handle to a queue that holds struct I2CCommand 
                               objects. This task issues commands to this queue 
                               to be handled by the I2C gatekeeper, implemented 
                               by vI2CGatekeeperTask. */
  ErrorResources *errRes; /*!< Holds global error handling resources. */
};

typedef struct DotWorkerTaskResources DotWorkerTaskResources;

/**
 * @brief Initializes the dot worker task, which is implemented by 
 *        vDotWorkerTask.
 * 
 * @note The dot worker task receives commands from the main task. It is the
 *       task that does the most 'business logic' of the application. It
 *       relieves the main task of these duties so that it can quickly respond
 *       to user input.
 * 
 * @param resources A collection of pointers to objects necessary for the
 *                  worker task to accept and fulfill commands.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t createDotWorkerTask(DotWorkerTaskResources *resources);

/**
 * Toggles the error LED to indicate that
 * an issue requesting traffic data from TomTom
 * has occurred, which is likely due to an invalid
 * or overused api key.
 */
void tomtomErrorTimerCallback(void *params);


void vDotWorkerTask(void *pvParameters);
void vOTATask(void* pvParameters);

#endif /* WORKER_H_ */