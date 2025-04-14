/**
 * action_task.h
 * 
 * Contains the action task, which is a task that schedules actions to run at
 * particular times of the day. The task exists separate from the ESP Timer
 * task because it runs low priority actions, whereas the ESP Timer runs at a
 * very high priority.
 */

#ifndef ACTION_TASK_H_4_9_25
#define ACTION_TASK_H_4_9_25

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_errors.h"

esp_err_t createActionTask(TaskHandle_t *handle);

#endif /* ACTION_TASK_H_4_9_25 */