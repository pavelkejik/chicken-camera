/***********************************************************************
 * Filename: deep_sleep_ctrl.cpp
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * 
 ***********************************************************************/

#include "Arduino.h"
#include "deep_sleep_ctrl.h"

SemaphoreHandle_t write_cmd_sem = xSemaphoreCreateBinary();

bool active_tasks[NUMBER_TASKS];
TaskHandle_t active_task_handle[NUMBER_TASK_HANDLES];


bool IsSystemIdle(void)
{
    for (int i = 0; i < NUMBER_TASKS; i++)
    {
        if (active_tasks[i])
        {
            return false;
        }
    }
    return true;
}