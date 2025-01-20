/**
 * verifier.h
 */

#ifndef VERIFIER_H_
#define VERIFIER_H_

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include "unity.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct VerificationResources {
    SemaphoreHandle_t sema;
    bool *correct;
    bool *waiting;
};

typedef struct VerificationResources VerificationResources;

esp_err_t initializeVerificationButtons(VerificationResources *resources);
void assertHumanVerifies(char *message, bool expected, VerificationResources res);

#endif /* VERIFIER_H_ */