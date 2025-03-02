/***********************************************************************
 * Filename: log.cpp
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Implements the SystemLog class, providing functions for 
 *     handling system logs. The log queue is managed using FreeRTOS to 
 *     facilitate efficient log handling, while the log entries are 
 *     stored in text files using LittleFS.
 *
 ***********************************************************************/

#include "log.h"
#include "common.h"
#include <LittleFS.h>
#include "freertos/queue.h"
#include "esp_now_ctrl.h"
#include "deep_sleep_ctrl.h"
#include "esp_now_client.h"

uint8_t SystemLog::write_file;
QueueHandle_t SystemLog::log_queue;

const char *const SystemLog::log_files[] = {
    "/log_a.txt",
    "/log_b.txt",
};

void SystemLog::printItem(const Log_t *item)
{
    const char *verb_text[] = {"", "[ERROR]", "[WARN]", "[INFO]"};
    char buffer[30];
    struct tm tm_info = GetTime(item->time);
    strftime(buffer, 30, "%d.%m.%Y %H:%M:%S", &tm_info);
    Serial.printf("%s %s %s\n", verb_text[item->lvl], buffer, item->log_txt);
}

void SystemLog::PutLog(const char *msg, Verbosity_t lvl, time_t t)
{
    Log_t log_item;
    log_item.lvl = lvl;
    log_item.time = (t != 0) ? t : Now();
    strncpy(log_item.log_txt, msg, sizeof(log_item.log_txt) - 1);
    log_item.log_txt[sizeof(log_item.log_txt) - 1] = 0;

    printItem(&log_item);

    xQueueSend(log_queue, &log_item, (TickType_t)0);
}

void SystemLog::PutLog(String msg, Verbosity_t lvl, time_t t)
{
    PutLog(msg.c_str(), lvl, t);
}

void SystemLog::Init(void)
{
    active_tasks[FileSystem_Task] = true;

    log_queue = xQueueCreate(5, sizeof(Log_t));

    size_t file_items = 0;
    std::lock_guard<std::mutex> lock(storageFS_lock);
    File file = storageFS.open(log_files[0], "r");
    write_file = 0;
    if (file)
    {
        file_items = file.size() / sizeof(Log_t);
        if (file_items >= NMR_RECORDS)
        {
            write_file = 1;
        }
        file.close();
    }

    file = storageFS.open(log_files[1], "r");
    if (file)
    {
        file_items = file.size() / sizeof(Log_t);

        if (file_items >= NMR_RECORDS)
        {
            write_file = 0;
        }
        file.close();
    }
}

void SystemLog::Task(void)
{
    active_tasks[FileSystem_Task] = false;

    Log_t log_item;

    if (xQueueReceive(log_queue, &(log_item), (TickType_t)pdMS_TO_TICKS(10000)) == pdTRUE)
    {
        active_tasks[FileSystem_Task] = true;
        std::lock_guard<std::mutex> lock(storageFS_lock);

        File file = storageFS.open(log_files[write_file], "a");
        if (!file)
        {
            file = storageFS.open(log_files[write_file], "w", true);
        }
        if (file)
        {
            size_t file_items = file.size() / sizeof(Log_t);
            if (file_items > NMR_RECORDS)
            {
                file.close();
                write_file ^= 1;
                file = storageFS.open(log_files[write_file], "w", true);
            }
            file.write((uint8_t *)&log_item, sizeof(Log_t));
            file.close();
        }
    }
}

size_t SystemLog::GetLogJson(JsonArray doc, size_t pos, size_t nmr_max)
{
    std::lock_guard<std::mutex> lock(storageFS_lock);

    size_t total_nmr = 0;

    int read_file = write_file ^ 1;
    size_t nmr;
    size_t file_items;
    File file = storageFS.open(log_files[read_file], "r");
    file_items = file.size() / sizeof(Log_t);
    total_nmr += file_items;
    if (pos < file_items)
    {
        nmr = file_items - pos;
        if (nmr >= nmr_max)
        {
            nmr = nmr_max;
            nmr_max = 0;
        }
        else
        {
            nmr_max = nmr_max - nmr;
        }
        file.seek(pos * sizeof(Log_t));
        while (nmr > 0)
        {
            Log_t log_item;
            size_t nmr_bytes = file.readBytes((char *)&log_item, sizeof(Log_t));
            if (nmr_bytes != sizeof(Log_t))
            {
                break;
            }
            JsonDocument obj;
            obj["v"] = log_item.lvl;
            obj["t"] = log_item.time;
            obj["msg"] = log_item.log_txt;
            doc.add(obj);
            nmr--;
        }
        pos = 0;
    }
    else
    {
        pos = pos - file_items;
    }
    file.close();

    file = storageFS.open(log_files[write_file], "r");
    file_items = file.size() / sizeof(Log_t);
    total_nmr += file_items;
    if (nmr_max > 0)
    {
        if ((pos + nmr_max) > file_items)
        {
            nmr_max = file_items - pos;
        }

        file.seek(pos * sizeof(Log_t));
        while (nmr_max > 0)
        {
            Log_t log_item;
            size_t nmr_bytes = file.readBytes((char *)&log_item, sizeof(Log_t));
            if (nmr_bytes != sizeof(Log_t))
            {
                break;
            }
            JsonDocument obj;
            obj["v"] = log_item.lvl;
            obj["t"] = log_item.time;
            obj["msg"] = log_item.log_txt;
            doc.add(obj);
            nmr_max--;
        }
    }
    file.close();

    return total_nmr;
}


bool SystemLog::SendLogsViaEspNow(const uint8_t *mac_addr)
{
    std::lock_guard<std::mutex> lock(storageFS_lock);
    size_t total_nmr = 0;
    DataPayload payload;
    bool sendMessageSuccess = true;

    memset(&payload, 0, sizeof(payload));
    size_t payloadFillIndex = 0;
    size_t currentIndex = 0;

    int read_file = write_file ^ 1;

    for (int i = 0; i < 2 && sendMessageSuccess; ++i)
    {
        int fileIndex = (read_file + i) % 2;

        File file = storageFS.open(log_files[fileIndex], "r");
        if (!file)
            continue;

        while (file.available() > 0 && sendMessageSuccess)
        {
            Log_t log_item;
            size_t readSize = file.readBytes((char *)&log_item, min(sizeof(Log_t), sizeof(payload.data) - payloadFillIndex));

            if (readSize > 0)
            {
                memcpy(payload.data + payloadFillIndex, &log_item, readSize);
                payloadFillIndex += readSize;
                currentIndex += readSize;

                if (payloadFillIndex == sizeof(payload.data))
                {
                    payload.index = currentIndex - payloadFillIndex;
                    payload.nmr = payloadFillIndex;

                    CHECK_SEND(ESPNowCtrl::SendMessage(mac_addr, MSG_GET_LOG_RESPONSE, payload, 4 + 1 + payload.nmr, 5), sendMessageSuccess);
                    total_nmr += payloadFillIndex / sizeof(Log_t);

                    memset(&payload, 0, sizeof(payload));
                    payloadFillIndex = 0;
                }
            }
        }

        file.close();
    }

    if (payloadFillIndex > 0 && sendMessageSuccess)
    {
        payload.index = currentIndex - payloadFillIndex;
        payload.nmr = payloadFillIndex;
        CHECK_SEND(ESPNowCtrl::SendMessage(mac_addr, MSG_GET_LOG_RESPONSE, payload, 4 + 1 + payload.nmr, 5), sendMessageSuccess);
        total_nmr += payloadFillIndex / sizeof(Log_t);
    }

    return sendMessageSuccess;
}

void SystemLog::Sleep(void)
{
    storageFS.end();
}