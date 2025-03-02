/***********************************************************************
 * Filename: button.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Declares the Button class, which manages input button operations, 
 *     including detecting normal and long presses.
 * 
 ***********************************************************************/

#pragma once
#include <Arduino.h>

#define STD_CNT 3
#define LONG_PUSH_TIME 2000

class Button
{
private:
    uint8_t pin;
    uint8_t mode;
    uint32_t switchtime;
    uint32_t stdcount;
    uint32_t long_push_time;
    bool push;
    bool longPush;
    bool press;
    bool pressLong;

public:
    Button(uint8_t p, uint8_t m, uint32_t long_push = LONG_PUSH_TIME)
    {
        pin = p;
        mode = m;
        long_push_time = long_push;
    }

    void Init(void)
    {
        pinMode(pin, mode);
        push = false;
        press = false;
        pressLong = false;
        longPush = false;
        stdcount = 0;
    }
    void Read(void)
    {

            if (digitalRead(pin) == LOW)
            {
                if (stdcount < STD_CNT)
                {
                    stdcount++;
                }
                else if (!push)
                {
                    push = true;
                    // press = true;
                    switchtime = millis();
                    // stdcount++;
                }
                if (push && !longPush)
                {
                    if ((millis() - switchtime) > long_push_time)
                    {
                        longPush = true;
                        pressLong = true;
                    }
                }
            }
            else
            {
                if (stdcount > 0)
                {
                    stdcount--;
                }
                else
                {
                    if (((millis() - switchtime) < long_push_time) && push)
                        press = true;

                    push = false;
                    longPush = false;
                }
            }
    }

    bool IsOn(void)
    {
        return push;
    }
    bool IsOnEvent(void)
    {
        bool ret = press;
        press = false;
        return ret;
    }
    bool IsLongOn(void)
    {
        return longPush;
    }
    bool IsLongOnEvent(void)
    {
        bool ret = pressLong;
        pressLong = false;
        return ret;
    }
};
