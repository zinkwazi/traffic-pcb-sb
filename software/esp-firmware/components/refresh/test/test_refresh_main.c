/**
 * test_refresh_main.c
 * 
 * This app runs unit tests for the refresh component.
 */

#include "unity.h"

#include "freertos/FreeRTOS.h"

#define TAG "test_refresh"

void setUp(void)
{

}

void tearDown(void)
{

}

void app_main(void)
{
    portDISABLE_INTERRUPTS();
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
    portENABLE_INTERRUPTS();
    for (;;) {}
}