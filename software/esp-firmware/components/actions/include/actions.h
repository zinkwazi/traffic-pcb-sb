/**
 * actions.h
 * 
 * Contains functions abstracting interaction with the action task, which
 * holds ownership over scheduling jobs and interacting with SNTP.
 */

#ifndef ACTIONS_H_4_7_25
#define ACTIONS_H_4_7_25

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"

#include "app_errors.h"

#include "action_task.h"

/**
 * @note To create a new action, add an option to the enum below and register
 * a handler with handleAction in actions.c.
 */

enum Action {
    ACTION_UPDATE_DATA,
    ACTION_UPDATE_BRIGHTNESS,
    ACTION_QUERY_OTA,
    ACTION_START_NIGHTTIME_MODE,
    ACTION_END_NIGHTTIME_MODE,
    ACTION_NONE, // used to denote that no actions are scheduled
};

typedef enum Action Action;

struct ScheduledAction {
    const time_t *schedule;
    const size_t scheduleLen;
    const Action action;
};

typedef struct ScheduledAction ScheduledAction;

/* used by action task */
int64_t getUpdateTrafficDataPeriodSec(void);
int64_t getUpdateBrightnessPeriodSec(void);
const ScheduledAction *getScheduledActions(void);
size_t getScheduledActionsLen(void);
esp_err_t handleAction(Action action);

#endif /* ACTIONS_H_4_7_25 */