/***********************************************************************
 * Filename: log.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Declares the SystemLog class for managing system logs. The class
 *     provides methods to initialize the log system, add log entries,
 *     and retrieve logs in JSON format and sending over ESP-NOW. It uses a FreeRTOS queue to
 *     manage log entries efficiently.
 *
 ***********************************************************************/

#pragma once
#include "Arduino.h"
#include "parameters.h"
#include "freertos/queue.h"

#define NMR_RECORDS 25

typedef struct
{
    Verbosity_t lvl;
    time_t time;
    char log_txt[80];
}__attribute__((packed)) Log_t;

class SystemLog
{
private:
    static void printItem(const Log_t *item);

    static QueueHandle_t log_queue;

    static const char *const log_files[];
    static uint8_t write_file;

public:
    static void PutLog(const char * msg, Verbosity_t lvl = v_info, time_t t = 0);
    static void PutLog(String msg, Verbosity_t lvl = v_info, time_t t = 0);

    static void Init(void);

    static void Task(void);

    static void WriteLock(void);

    static size_t GetLogJson(JsonArray doc, size_t pos, size_t nmr_max);

    static bool SendLogsViaEspNow(const uint8_t *mac_addr);

    static void Sleep(void);
};