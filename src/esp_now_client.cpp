/***********************************************************************
 * Filename: esp_now_client.cpp
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * 
 ***********************************************************************/

#include "esp_now_client.h"

SemaphoreHandle_t ESPNowClient::semaphore = xSemaphoreCreateBinary();
bool ESPNowClient::gotMasterResponse;
std::mutex ESPNowClient::mutex;
bool ESPNowClient::isUpdating = false;
uint32_t ESPNowClient::startUpdateTime;
bool ESPNowClient::param_defs_send = false;
bool ESPNowClient::picture_send = false;