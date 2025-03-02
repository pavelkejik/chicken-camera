/***********************************************************************
 * Filename: esp_now_ctrl.cpp
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Implements the ESPNowCtrl class, which provides functionality 
 *     for initializing ESP-NOW communication, sending and receiving 
 *     messages, and managing peers. The class handles data reception 
 *     and transmission using callbacks and FreeRTOS queues.
 *
 ***********************************************************************/

#include "esp_now_ctrl.h"
#include "WiFi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "log.h"

uint8_t BroadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
DataReceivedCallback ESPNowCtrl::onDataReceivedCallback;
DataSentCallback ESPNowCtrl::onDataSentCallback;
QueueHandle_t ESPNowCtrl::sendQueue = NULL;
QueueHandle_t ESPNowCtrl::receiveQueue = NULL;

bool ESPNowCtrl::initDone = false;

void ESPNowCtrl::Init()
{
    if (initDone)
    {
        return;
    }
    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        SystemLog::PutLog("Error initializing ESP-NOW", v_error);
        return;
    }
    esp_now_set_wake_window(UINT16_MAX);

    if (sendQueue == NULL)
    {
        sendQueue = xQueueCreate(10, sizeof(esp_now_send_status_t));
    }
    if (receiveQueue == NULL)
    {
        receiveQueue = xQueueCreate(100, sizeof(ESPNowItem_t));
    }

    esp_now_register_recv_cb(onDataRecv);

    esp_now_register_send_cb(onDataSent);
    AddPeer(BroadcastAddress, 0);

    initDone = true;
}

void ESPNowCtrl::Deinit()
{
    if (!initDone)
    {
        return;
    }
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    initDone = false;
}

void ESPNowCtrl::AddPeer(const uint8_t *mac_addr, uint8_t chan)
{
    DeletePeer(mac_addr);
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    peer.channel = chan;
    peer.encrypt = false;
    // peer.ifidx = WIFI_IF_STA;
    memcpy(peer.peer_addr, mac_addr, sizeof(uint8_t[6]));
    if (esp_now_add_peer(&peer) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }
}

void ESPNowCtrl::DeletePeer(const uint8_t *mac_addr)
{
    esp_now_del_peer(mac_addr);
}

void ESPNowCtrl::SetChannel(uint8_t channel)
{
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

bool ESPNowCtrl::SendMessageInternal(const uint8_t *peer_addr, uint8_t messageType, const uint8_t *payload, uint8_t payloadSize, uint8_t retryCount)
{
    for (uint8_t retries = 0; retries < retryCount; retries++)
    {
        SendMessageRaw(peer_addr, messageType, payload, payloadSize);

        esp_now_send_status_t status;
        if (xQueueReceive(sendQueue, &status, (TickType_t)pdMS_TO_TICKS(1000)) == pdPASS)
        {
            if (status == ESP_NOW_SEND_SUCCESS)
            {
                return true;
            }
            delay(50);
        }
        else
        {
            Serial.println("Timeout waiting for send status.");
        }
    }
    return false; // Failed to send after retrying
}

bool ESPNowCtrl::SendMessage(const uint8_t *peer_addr, uint8_t messageType, uint8_t retryCount)
{
    return SendMessageInternal(peer_addr, messageType, nullptr, 0, retryCount);
}

void ESPNowCtrl::SendMessageRaw(const uint8_t *peer_addr, uint8_t messageType, const uint8_t *payloadData, uint8_t payloadSize)
{
    if (payloadSize > (MAX_PAYLOAD_SIZE))
    {
        Serial.printf("Payload size is too large: %d bytes, Max allowed: %d bytes\n", payloadSize, MAX_PAYLOAD_SIZE);
        return;
    }

    Message msg;
    msg.messageType = messageType;
    msg.payloadSize = payloadSize;
    memcpy(msg.payload, payloadData, payloadSize);

    size_t totalMessageSize = sizeof(msg.messageType) + sizeof(msg.payloadSize) + payloadSize;
    esp_now_send(peer_addr, (uint8_t *)&msg, totalMessageSize);
}

void ESPNowCtrl::SetDataReceivedCallback(DataReceivedCallback callback)
{
    onDataReceivedCallback = callback;
}

void ESPNowCtrl::SetDataSentCallback(DataSentCallback callback)
{
    onDataSentCallback = callback;
}

void ESPNowCtrl::onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
    if (len <= sizeof(Message))
    {
        ESPNowItem_t msg;
        memcpy(msg.mac_addr, mac_addr, 6);
        memcpy(msg.data, incomingData, len);
        msg.len = len;

        xQueueSendToBack(receiveQueue, &msg, pdMS_TO_TICKS(2000));
    }
}

void ESPNowCtrl::Task()
{
    ESPNowItem_t data;

    if (xQueueReceive(receiveQueue, &data, portMAX_DELAY) == pdTRUE)
    {
        if (onDataReceivedCallback != NULL)
        {
            if (data.len < (sizeof(Message) - MAX_PAYLOAD_SIZE))
            {
                SystemLog::PutLog("ESP-Now data too short", v_warning);
                return;
            }
            Message *msg = (Message *)data.data;
            if (data.len != (sizeof(Message) - MAX_PAYLOAD_SIZE + msg->payloadSize))
            {
                SystemLog::PutLog("ESP-Now data incorrect length", v_error);
                return;
            }
            onDataReceivedCallback(data.mac_addr, msg, data.len);
        }
    }
}

void ESPNowCtrl::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    // if (onDataSentCallback != NULL)
    // {
    //     onDataSentCallback(mac_addr, status);
    // }
    xQueueSendToBack(sendQueue, &status, portMAX_DELAY);
}

void ESPNowCtrl::SetPower(wifi_power_t power)
{
    WiFi.setTxPower(power);
}