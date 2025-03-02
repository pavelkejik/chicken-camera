/***********************************************************************
 * Filename: deep_sleep_ctrl.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Declares functions and variables for controlling deep sleep mode,
 *     checking task activity, and managing active tasks in the system.
 ***********************************************************************/

#pragma once

#include "Arduino.h"

#define NUMBER_TASK_HANDLES 3

typedef enum
{
    FileSystem_Task,
    Communication_Task,
    Camera_Task,

    NUMBER_TASKS
} ActiveTask_t;

extern bool active_tasks[NUMBER_TASKS];
extern TaskHandle_t active_task_handle[NUMBER_TASK_HANDLES];


extern SemaphoreHandle_t write_cmd_sem;

bool IsSystemIdle(void);