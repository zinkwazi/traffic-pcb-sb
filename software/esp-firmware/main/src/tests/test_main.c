/**
 * test_main.c
 * 
 * Created On: 8/28/26
 * Author: Jaden Baptista
 * 
 * This app runs unit tests. Configuration of tests is done at the build level.
 */

#include "unity.h"

#include "freertos/FreeRTOS.h"

#define TAG "test_main"

void setUp(void)
{

}

void tearDown(void)
{

}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
    for (;;) {}
}