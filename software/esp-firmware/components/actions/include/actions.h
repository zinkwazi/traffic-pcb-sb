/**
 * actions.h
 * 
 * Contains functions abstracting interaction with the action task, which
 * holds ownership over scheduling jobs and interacting with SNTP.
 */

#ifndef ACTIONS_H_4_7_25
#define ACTIONS_H_4_7_25

#include "esp_err.h"

esp_err_t initJobs(void);

#endif /* ACTIONS_H_4_7_25 */