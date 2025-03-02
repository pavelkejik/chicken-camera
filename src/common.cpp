/***********************************************************************
 * Filename: common.cpp
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Implements utility functions declared in common.h.
 * 
 ***********************************************************************/

#include "common.h"

void SetTimezone(const char *tz)
{
    if (tz != NULL)
    {
        setenv("TZ", tz, 1);
        tzset();
    }
}

time_t Now(void)
{
    return time(NULL);
}

struct tm GetTime(time_t t)
{
    struct tm tmp;
    localtime_r(&t, &tmp);
    return tmp;
}

struct tm GetTime(void)
{
    return GetTime(Now());
}

int Year(void)
{
    struct tm tmp = GetTime();
    return tmp.tm_year + 1900;
}

int Month(void)
{
    struct tm tmp = GetTime();
    return tmp.tm_mon + 1;
}

int Day(void)
{
    struct tm tmp = GetTime();
    return tmp.tm_mday;
}

int Hour(void)
{
    struct tm tmp = GetTime();
    return tmp.tm_hour;
}

int Minute(void)
{
    struct tm tmp = GetTime();
    return tmp.tm_min;
}

int Second(void)
{
    struct tm tmp = GetTime();
    return tmp.tm_sec;
}

bool IsDST(void)
{
    struct tm tmp = GetTime();
    return tmp.tm_isdst == 1;
}

int Weekday(void)
{
    struct tm tmp = GetTime();
    return tmp.tm_wday + 1;
}

void SetDateTime(time_t t)
{
    struct timeval nowt = {.tv_sec = t};
    settimeofday(&nowt, NULL);
}

time_t MakeEpochTime(int year, int month, int day, int hour, int min)
{
    struct tm desired_time;
    desired_time.tm_year = year - 1900;
    desired_time.tm_mon = month - 1;
    desired_time.tm_mday = day;
    desired_time.tm_hour = hour;
    desired_time.tm_min = min;
    desired_time.tm_sec = 0;
    desired_time.tm_isdst = -1;
    return mktime(&desired_time);
}

void SetDateTime(int year, int month, int day, int hours, int minutes)
{
    time_t set = MakeEpochTime(year, month, day, hours, minutes);
    struct timeval nowt = {.tv_sec = set};
    settimeofday(&nowt, NULL);
}

bool EndTimer(uint32_t &timer)
{
    if (timer != 0)
    {
        --timer;
        if (timer == 0)
        {
            return true;
        }
    }
    return false;
}

void StopTimer(uint32_t &timer)
{
    timer = 0;
}

void StartTimer(uint32_t &timer, uint32_t value)
{
    timer = value;
}

bool IsTimerRunning(const uint32_t &timer)
{
    return timer != 0;
}

void StringToMac(const String &macString, uint8_t *macAddress)
{
    if (macString.length() != 17)
    {
        return;
    }

    sscanf(macString.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
           &macAddress[0], &macAddress[1], &macAddress[2],
           &macAddress[3], &macAddress[4], &macAddress[5]);
}

String MacToString(const uint8_t *macAddress)
{
    char macStr[18]; // Buffer to hold the MAC address string
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             macAddress[0], macAddress[1], macAddress[2],
             macAddress[3], macAddress[4], macAddress[5]);
    return String(macStr);
}

fs::LittleFSFS storageFS;
std::mutex storageFS_lock;