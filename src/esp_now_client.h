/***********************************************************************
 * Filename: esp_now_client.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Defines the ESPNowClient class, which serves as a client in an
 *     ESP-NOW network. It manages communication with the network's master
 *     node, handles pairing, data transfer, and over-the-air updates.
 *     The class provides methods to send and receive data packets,
 *     handle firmware updates, and manage device states and parameters.
 *
 ***********************************************************************/

#pragma once
#include "Arduino.h"
#include "parameters.h"
#include "log.h"
#include "esp_now_ctrl.h"
#include <Update.h>
#include "deep_sleep_ctrl.h"
#include "camera.h"

#define COMMUNICATION_ATTEMPTS 2
#define DEVICE_TYPE DEVICE_TYPE_EGG_CAMERA
#define UPDATE_TIMEOUT_S 100

#define CHECK_SEND(functionCall, resultVar)          \
    do                                               \
    {                                                \
        resultVar = true;                            \
        int _attempts = 0;                           \
        while (!functionCall)                        \
        {                                            \
            Serial.println("Communication error");   \
            _attempts++;                             \
            if (_attempts >= COMMUNICATION_ATTEMPTS) \
            {                                        \
                if (ESPNowClient::ScanForMaster())   \
                {                                    \
                    resultVar = functionCall;        \
                }                                    \
                else                                 \
                {                                    \
                    resultVar = false;               \
                }                                    \
                break;                               \
            }                                        \
        }                                            \
    } while (0)

#define CHECK_SEND_RETURN_IF_FAIL(functionCall) \
    do                                          \
    {                                           \
        bool result;                            \
        CHECK_SEND(functionCall, result);       \
        if (!result)                            \
        {                                       \
            return false;                       \
        }                                       \
    } while (0)

#define CHECK_SEND_BREAK_IF_FAIL(functionCall) \
                                               \
    {                                          \
        bool _result;                          \
        CHECK_SEND(functionCall, _result);     \
        if (!_result)                          \
        {                                      \
            break;                             \
        }                                      \
    }

#define CHECK_RETURN_IF_FAIL(functionCall) \
    do                                     \
    {                                      \
        bool _result = functionCall;       \
        if (!_result)                      \
        {                                  \
            return false;                  \
        }                                  \
    } while (0)

#define CHECK_BREAK_IF_FAIL(functionCall) \
                                          \
    {                                     \
        bool _result = functionCall;      \
        if (!_result)                     \
        {                                 \
            break;                        \
        }                                 \
    }

class ESPNowClient
{
private:
    static bool gotMasterResponse;
    static SemaphoreHandle_t semaphore;
    static std::mutex mutex;
    static bool isUpdating;
    static uint32_t startUpdateTime;
    static bool param_defs_send;
    static bool picture_send;

    static bool sendParamDefs(const uint8_t *mac_addr)
    {
        ParamDefsPayload payload;
        uint8_t parIdx = 0;
        bool res = true;
        for (int i = 0; i < Register::NmrParameters; i++)
        {
            Register *reg = Register::GetParByIdx(i);
            if (reg && (reg->def.dsc & Par_ESPNow))
            {
                payload.params[parIdx].adr = reg->def.adr;
                payload.params[parIdx].min = reg->def.min;
                payload.params[parIdx].max = reg->def.max;
                payload.params[parIdx].dsc = reg->def.dsc;
                payload.params[parIdx].flags = reg->def.atr;
                strncpy(payload.params[parIdx].ptxt, reg->def.ptxt, sizeof(payload.params[parIdx].ptxt) - 1);
                payload.params[parIdx].ptxt[sizeof(payload.params[parIdx].ptxt) - 1] = '\0';
                parIdx++;
                payload.numParams = parIdx;
                if (parIdx >= MAX_PARAM_DEFS)
                {
                    CHECK_SEND_RETURN_IF_FAIL(ESPNowCtrl::SendMessage(mac_addr, MSG_GET_PARAM_DEFS_RESPONSE, payload, sizeof(pardef_t_espnow) * MAX_PARAM_DEFS + 1));

                    parIdx = 0;
                    memset(&payload, 0, sizeof(ParamDefsPayload));
                }
            }
        }
        if (parIdx > 0)
        {
            CHECK_SEND_RETURN_IF_FAIL(ESPNowCtrl::SendMessage(mac_addr, MSG_GET_PARAM_DEFS_RESPONSE, payload, sizeof(pardef_t_espnow) * parIdx + 1));
        }
        return res;
    }

    static bool sendParamValues(const uint8_t *mac_addr)
    {
        ReadRequestPayload payload;
        uint16_t regFirst = UINT16_MAX;
        uint16_t regLast = 0;
        for (int i = 0; i < Register::NmrParameters; i++)
        {
            Register *reg = Register::GetParByIdx(i);
            if (reg && (reg->def.dsc & Par_ESPNow))
            {
                if (reg->def.adr < regFirst)
                {
                    regFirst = reg->def.adr;
                }
                if ((reg->def.adr + reg->getsize()) > regLast)
                {
                    regLast = reg->def.adr + reg->getsize();
                }
            }
        }
        if (regFirst != UINT16_MAX)
        {
            payload.regAddr = regFirst;
            payload.nmr = regLast - regFirst;
            return readParamsRequestHandler(mac_addr, &payload);
        }
        return true;
    }

    static bool readParamsRequestHandler(const uint8_t *mac_addr, const ReadRequestPayload *payload)
    {
        uint16_t regadr = payload->regAddr;
        uint16_t regnmr = payload->nmr;
        ReadResponsePayload response;
        uint16_t adrIdx = 0;
        uint16_t reglast = regadr + regnmr;
        bool res = true;
        response.regAddr = regadr;
        for (uint16_t i = regadr; i < reglast; i++)
        {
            int16_t regval;
            Register::ReadReg(&regval, i);
            response.values[adrIdx] = regval;
            adrIdx++;
            response.nmr = adrIdx;
            if (adrIdx >= MAX_PARAM_READS_WRITES)
            {
                CHECK_SEND_RETURN_IF_FAIL(ESPNowCtrl::SendMessage(mac_addr, MSG_READ_PARAM_RESPONSE, response, 4 + adrIdx * 2));
                adrIdx = 0;
                memset(&response, 0, sizeof(ReadResponsePayload));
                response.regAddr = i + 1;
            }
        }

        if (adrIdx > 0)
        {
            CHECK_SEND_RETURN_IF_FAIL(ESPNowCtrl::SendMessage(mac_addr, MSG_READ_PARAM_RESPONSE, response, 4 + adrIdx * 2));
        }
        return res;
    }

    static void writeParamsRequestHandler(const uint8_t *mac_addr, const WriteRequestPayload *payload)
    {
        uint16_t regadr = payload->regAddr;
        uint16_t regnmr = payload->nmr;

        for (uint16_t i = 0; i < regnmr; i++)
        {
            Register::WriteReg(payload->values[i], i + regadr);
        }
    }

    static bool pairResponseHandler(const uint8_t *mac_addr, const PairResponsePayload *payload)
    {
        bool res = true;
        if (payload->state == PAIR_STATE_PAIRED)
        {
            bool isBroadcast = false;
            gotMasterResponse = true;
            {
                std::lock_guard<std::mutex> lock(mutex);
                isBroadcast = memcmp(MasterMacAdresa.Get(), BroadcastAddress, 6) == 0;
                MasterMacAdresa.Set(mac_addr);
                WiFiKanal.Set(payload->channel);
            }
            ESPNowCtrl::AddPeer(mac_addr, 0);
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            Serial.printf("Paired on channel: %d, MAC addr: %s\n", payload->channel, macStr);

            if (isBroadcast)
            {
                StavZarizeni.Set(Sparovano);
                CHECK_RETURN_IF_FAIL(sendParamDefs(mac_addr));
                CHECK_SEND_RETURN_IF_FAIL(ESPNowCtrl::SendMessage(mac_addr, MSG_TIME_SYNC_REQUEST));
                CHECK_RETURN_IF_FAIL(sendParamValues(mac_addr));
                CHECK_SEND_RETURN_IF_FAIL(ESPNowCtrl::SendMessage(mac_addr, MSG_TRANSMIT_DONE));
            }
        }
        else if ((payload->state == PAIR_STATE_INITIAL_REQUEST) || (payload->state == PAIR_STATE_EXPIRED))
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (memcmp(mac_addr, MasterMacAdresa.Get(), 6) == 0)
                {
                    ESPNowCtrl::DeletePeer(mac_addr);
                    MasterMacAdresa.Set(BroadcastAddress);
                }
            }
            StavZarizeni.Set(Parovani);
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            Serial.printf("Find device on channel: %d, MAC addr: %s\n", payload->channel, macStr);
        }
        return res;
    }

    static void fwUpdateRequestHandler(const uint8_t *mac_addr, UpdateRequestPayload *payload)
    {
        static uint32_t last_index = 0;
        if (!payload->index)
        {
            active_tasks[Communication_Task] = true;
            SystemLog::PutLog("Start aktualizace firmwaru", v_info);
            isUpdating = true;
            startUpdateTime = millis();
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, payload->isFW ? U_FLASH : U_SPIFFS))
                Update.printError(Serial);
        }
        // Serial.printf("Get data, size: %d, index: %d, FW: %d\n", payload->nmr, payload->index, payload->isFW);
        if (Update.write(payload->data, payload->nmr) != payload->nmr)
            Update.printError(Serial);
        if (payload->isFinal)
        {
            if (!Update.end(true))
            {
                SystemLog::PutLog("Pri aktualizaci firmwaru doslo k chybe. Zarizeni se restartuje.", v_error);
                Update.printError(Serial);
                delay(1000);
            }
            else
            {
                SystemLog::PutLog("Aktualizace firmwaru probehla uspesne", v_info);
                delay(1000);
            }
            RestartCmd.Set(povoleno);
            isUpdating = false;
        }
    }

    static void timeSyncResponseHandler(const uint8_t *mac_addr, const TimeSyncPayload *payload)
    {
        PopisCasu.Set(payload->timezone);
        SetTimezone(payload->timezone);
        SetDateTime(payload->currentTime);
        CasVychodu.Set(payload->sunriseTime);
        CasZapadu.Set(payload->sunsetTime);
    }

    static void handleDataReceived(const uint8_t *mac_addr, const Message *msg, int len)
    {
        // Serial.printf("Received data type: %d, len: %d\n", msg->messageType, len);
        switch (msg->messageType)
        {
        case MSG_PAIR_RESPONSE:
            pairResponseHandler(mac_addr, (const PairResponsePayload *)(msg->payload));
            break;
        case MSG_GET_PARAM_DEFS_REQUEST:
            sendParamDefs(mac_addr);
            break;
        case MSG_READ_PARAM_REQUEST:
            readParamsRequestHandler(mac_addr, (const ReadRequestPayload *)(msg->payload));
            break;
        case MSG_WRITE_PARAM_REQUEST:
            writeParamsRequestHandler(mac_addr, (const WriteRequestPayload *)(msg->payload));
            Camera::Wake();
            break;

        case MSG_FW_UPDATE_REQUEST:
            fwUpdateRequestHandler(mac_addr, (UpdateRequestPayload *)(msg->payload));
            break;

        case MSG_TIME_SYNC_RESPONSE:
            timeSyncResponseHandler(mac_addr, (const TimeSyncPayload *)(msg->payload));
            break;

        case MSG_GET_LOG_REQUEST:
            SystemLog::SendLogsViaEspNow(mac_addr);
            break;

        case MSG_TRANSMIT_DONE:
            active_tasks[Communication_Task] = false;

            // xSemaphoreGive(semaphore);
            break;
        default:
            Serial.println("Received unknown data");
            break;
        }
    }

    static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
    {
    }

public:
    static void Init(void)
    {
        ESPNowCtrl::Init();
        ESPNowCtrl::SetDataReceivedCallback(handleDataReceived);
        ESPNowCtrl::SetDataSentCallback(OnDataSent);
        ESPNowCtrl::SetChannel(WiFiKanal.Get());
        ESPNowCtrl::AddPeer(MasterMacAdresa.Get(), 0);
        active_tasks[Communication_Task] = true;
    }

    static void Task(void)
    {
        bool res = true;
        if (!isUpdating)
        {
            if (RestartCmd.Get() == povoleno)
            {
                delay(1000);
            }

            active_tasks[Communication_Task] = true;

            bool pair;
            uint8_t mac_addr[6];
            {
                std::lock_guard<std::mutex> lock(mutex);
                memcpy(mac_addr, MasterMacAdresa.Get(), 6);
            }

            pair = memcmp(mac_addr, BroadcastAddress, 6) == 0;

            if (pair)
            {
                res = (StavZarizeni.Get() == Parovani) ? ScanForMaster() : false;
            }
            else
            {
                res = false;
                do
                {
                    if (ResetReason.Get() != rst_Deepsleep && !param_defs_send)
                    {
                        param_defs_send = true;
                        CHECK_BREAK_IF_FAIL(sendParamDefs(mac_addr));
                    }

                    if (picture_send)
                    {
                        Camera::SendPictureViaEspNow(mac_addr);
                        picture_send = false;
                        break;
                    }

                    CHECK_BREAK_IF_FAIL(sendParamValues(mac_addr));
                    CHECK_SEND_BREAK_IF_FAIL(ESPNowCtrl::SendMessage(mac_addr, MSG_TIME_SYNC_REQUEST));
                    CHECK_BREAK_IF_FAIL(SystemLog::SendLogsViaEspNow(mac_addr));
                    CHECK_SEND_BREAK_IF_FAIL(ESPNowCtrl::SendMessage(mac_addr, MSG_TRANSMIT_DONE));
                    Camera::Wake();
                    res = true;
                } while (0);
            }

            if (!isUpdating && !res)
            {
                active_tasks[Communication_Task] = false;
            }
        }
        else
        {
            if ((millis() - startUpdateTime) > (UPDATE_TIMEOUT_S * 1000))
            {
                SystemLog::PutLog("Pri aktualizaci firmwaru doslo k chybe: Timeout", v_error);
                delay(500);
                RestartCmd.Set(povoleno);
                active_tasks[Communication_Task] = false;
                isUpdating = false;
            }
        }
        if (xSemaphoreTake(semaphore, pdMS_TO_TICKS((5000))) != pdTRUE)
        {
            if (!isUpdating)
            {
                active_tasks[Communication_Task] = false;
                delay(1000);
            }
        }
    }

    static void StartPairing(void)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (memcmp(MasterMacAdresa.Get(), BroadcastAddress, 6))
            {
                ESPNowCtrl::DeletePeer(MasterMacAdresa.Get());
            }
            MasterMacAdresa.Set(BroadcastAddress);
            if (StavZarizeni.Get() == Parovani)
            {
                StavZarizeni.Set(NormalniMod);
            }
            else
            {
                StavZarizeni.Set(Parovani);
            }
        }
        xSemaphoreGive(semaphore);
    }

    static void SendPhoto(void)
    {
        active_tasks[Communication_Task] = true;
        picture_send = true;
        xSemaphoreGive(semaphore);
    }

    static bool ScanForMaster()
    {
        gotMasterResponse = false;
        uint8_t macAddr[6];
        {
            std::lock_guard<std::mutex> lock(mutex);
            memcpy(macAddr, MasterMacAdresa.Get(), 6);
        }
        WiFiKanal.Set(1);
        uint8_t channel = 1;
        while (!gotMasterResponse)
        {
            PairRequestPayload request;
            request.channel = channel;
            ESPNowCtrl::SetChannel(request.channel);
            delay(5);
            request.deviceType = DEVICE_TYPE;
            if (ESPNowCtrl::SendMessage(macAddr, MSG_PAIR_REQUEST, request, sizeof(PairRequestPayload)))
            {
                delay(200);
            }
            if (gotMasterResponse)
            {
                break;
            }
            channel++;
            if (channel > MAX_CHANNEL)
            {
                // WiFiKanal.Set(1);
                Serial.println("Pair scan done.");
                break;
            }
            // memcmp(MasterMacAdresa.Get(), BroadcastAddress, 6) == 0
        }
        delay(100);
        ESPNowCtrl::SetChannel(WiFiKanal.Get());
        delay(10);
        if (!gotMasterResponse)
        {
            Serial.println("Device not paired.");
        }
        return gotMasterResponse;
    }
};