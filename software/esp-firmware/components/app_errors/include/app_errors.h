/**
 * app_errors.h
 * 
 * Contains functions for raising error states to the user.
 */

#ifndef ERROR_H_
#define ERROR_H_

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

esp_err_t initAppErrors(void);
esp_err_t getAppErrorsStatus(void);
void throwNoConnError(void);
void throwHandleableError(void);
void throwFatalError(void);
void resolveNoConnError(bool resolveNone);
void resolveHandleableError(bool resolveNone);

#endif /* ERROR_H_ */