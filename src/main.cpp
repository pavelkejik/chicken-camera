/***********************************************************************
 * Filename: main.cpp
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     This file serves as the entry point for the application, 
 *     setting up and managing all hardware and software components. 
 *     It initializes modules such as ESP-NOW as well as handling device management and logging 
 *     The program controls various tasks for camera management, system logging and 
 *     deep sleep control, incorporating extensive use of FreeRTOS for task management.
 *
 ***********************************************************************/

#include <Arduino.h>
#include "button.h"
#include <esp_now.h>
#include <LittleFS.h>
#include "esp_now_client.h"
#include "FreeRTOSConfig.h"
#include "deep_sleep_ctrl.h"
#include "common.h"
#include "log.h"
#include "parameters.h"
#include "esp32/rom/rtc.h"
#include "pin_map.h"
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void SystemLogTask(void *pvParameters)
{
  while (true)
  {
    SystemLog::Task();
    delay(200);
  }
}

void ESPNowTask(void *pvParameters)
{
  while (true)
  {
    ESPNowCtrl::Task();
    // delayMicroseconds(100);
  }
}

void ESPNowSlaveTask(void *pvParameters)
{
  while (true)
  {
    ESPNowClient::Task();
  }
}

void CameraTask(void *pvParameters)
{
  while (true)
  {
    Camera::Task();
  }
}

void SleepTask(void *pvParameters)
{
  while (true)
  {
    if (IsSystemIdle())
    {
      vTaskSuspendAll();
      for (int i = 0; i < NUMBER_TASK_HANDLES; i++)
      {
        if (active_task_handle[i] != 0)
        {
          vTaskDelete(active_task_handle[i]);
        }
      }
      xTaskResumeAll();
      Register::Sleep();
      SystemLog::Sleep();
      if (RestartCmd.Get() == povoleno)
      {
        ESP.restart();
      }
      else
      {

        SleepPayload payload;
        payload.sleepTime = PeriodaKomunikace_S.Get();
        ESPNowCtrl::SendMessage(MasterMacAdresa.Get(), MSG_SLEEP, payload, sizeof(payload));

        esp_sleep_enable_timer_wakeup((uint64_t)PeriodaKomunikace_S.Get() * 1000000ULL);
        esp_deep_sleep_start();
      }
    }
    delay(10);
  }
}

void setup()
{
  Serial.begin(115200);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  memset(active_task_handle, 0, sizeof(active_task_handle));
  memset(active_tasks, 0, sizeof(active_tasks));

  Register::InitAll();
  storageFS.begin(true, "/storage", 5);
  SystemLog::Init();
  Camera::Init();
  // Camera::TakePicture();
  ESPNowClient::Init();

  switch (rtc_get_reset_reason(0))
  {
  case 1:
    ResetReason.Set(rst_Poweron);
    break;
  case 3:
  case 12:
    ResetReason.Set(rst_Software);
    break;
  case 5:
    ResetReason.Set(rst_Deepsleep);
    break;
  case 4:
  case 7:
  case 8:
  case 9:
  case 11:
  case 13:
  case 16:
    ResetReason.Set(rst_Watchdog);
    break;
  case 15:
    ResetReason.Set(rst_Brownout);
    break;

  case 14:
    ResetReason.Set(rst_External);

  default:
    ResetReason.Set(rst_Unknown);
  }

  xTaskCreateUniversal(SystemLogTask, "logTask", getArduinoLoopTaskStackSize(), NULL, 1, &active_task_handle[0], ARDUINO_RUNNING_CORE);
  xTaskCreateUniversal(ESPNowTask, "espNowTask", getArduinoLoopTaskStackSize(), NULL, 5, NULL, ARDUINO_RUNNING_CORE);
  xTaskCreateUniversal(ESPNowSlaveTask, "espNowSlaveTask", getArduinoLoopTaskStackSize(), NULL, 1, &active_task_handle[1], ARDUINO_RUNNING_CORE);
  xTaskCreateUniversal(SleepTask, "sleepTask", getArduinoLoopTaskStackSize(), NULL, 1, NULL, ARDUINO_RUNNING_CORE);
  xTaskCreateUniversal(CameraTask, "cameraTask", getArduinoLoopTaskStackSize(), NULL, 1, &active_task_handle[2], ARDUINO_RUNNING_CORE);
}

void loop()
{
  delay(5000);
}