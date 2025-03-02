/***********************************************************************
 * Filename: esp_now_ctrl.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Declares the ESPNowCtrl class, which provides functions for 
 *     managing ESP-NOW communication. The class includes methods for 
 *     initializing ESP-NOW, sending and receiving messages, and 
 *     managing peers. It defines various message types, payload 
 *     structures, and function pointers for handling data received 
 *     and data sent events.
 *
 ***********************************************************************/

#pragma once
#include "Arduino.h"
#include "esp_now.h"
#include "freertos/semphr.h"
#include "WiFiGeneric.h"

#define MAX_PAYLOAD_SIZE 240
#define MAX_PACKET_SIZE 250
#define MAX_CHANNEL 13
#define MAX_PARAM_DEFS 5
#define MAX_PARAM_READS_WRITES 118

extern uint8_t BroadcastAddress[];

typedef enum
{
    DEVICE_TYPE_DOOR_CONTROL = 0,
    DEVICE_TYPE_FEEDER = 1,
    DEVICE_TYPE_CAMERA = 2,
} DeviceType_t;

typedef enum
{
    MSG_NACK = 0,
    MSG_TRANSMIT_DONE = 1,
    MSG_PAIR_REQUEST = 2,
    MSG_PAIR_RESPONSE,
    MSG_READ_PARAM_REQUEST,
    MSG_READ_PARAM_RESPONSE,
    MSG_WRITE_PARAM_REQUEST,
    MSG_WRITE_PARAM_RESPONSE,
    MSG_GET_PARAM_DEFS_REQUEST,
    MSG_GET_PARAM_DEFS_RESPONSE,
    MSG_FW_UPDATE_REQUEST,
    MSG_FW_UPDATE_RESPONSE,
    MSG_GET_LOG_REQUEST,
    MSG_GET_LOG_RESPONSE,
    MSG_TIME_SYNC_REQUEST,
    MSG_TIME_SYNC_RESPONSE,
    MSG_SLEEP,
    MSG_BYTE_STREAM,
    MSG_DISCOVERY,
    MSG_ACK,
} MessageType_t;

typedef enum
{
    PAIR_STATE_INITIAL_REQUEST = 0,
    PAIR_STATE_APPROVED,
    PAIR_STATE_PAIRED,
    PAIR_STATE_EXPIRED
} PairingState_t;

typedef struct
{
    uint8_t messageType;
    uint8_t payloadSize;
    uint8_t payload[MAX_PAYLOAD_SIZE];
} __attribute__((packed)) Message;

typedef struct
{
    uint8_t deviceType;
    uint8_t channel;
} __attribute__((packed)) PairRequestPayload;

typedef struct
{
    uint8_t deviceType;
    uint8_t channel;
    uint8_t state;
} __attribute__((packed)) PairResponsePayload;

typedef struct
{
    uint8_t mac_addr[6];
    int len;
    uint8_t data[MAX_PACKET_SIZE];
} ESPNowItem_t;

typedef struct
{
    int32_t min;
    int32_t max;
    uint32_t dsc;
    uint16_t adr;
    uint8_t flags;
    char ptxt[32]; 
} __attribute__((packed)) pardef_t_espnow;

typedef struct
{
    uint8_t numParams;                      
    pardef_t_espnow params[MAX_PARAM_DEFS]; 
} __attribute__((packed)) ParamDefsPayload;

typedef struct
{
    uint16_t regAddr;
    uint16_t nmr;
} __attribute__((packed)) ReadRequestPayload;

typedef struct
{
    uint16_t regAddr;
    uint16_t nmr;
    int16_t values[MAX_PARAM_READS_WRITES];
} __attribute__((packed)) ReadResponsePayload;

typedef struct
{
    uint16_t regAddr;
    uint16_t nmr;
    int16_t values[MAX_PARAM_READS_WRITES];
} __attribute__((packed)) WriteRequestPayload;

typedef struct
{
    uint32_t index;
    uint8_t nmr;
    union
    {
        uint8_t atr;
        struct
        {
            uint8_t isFinal : 1;
            uint8_t isFW : 1;
            uint8_t : 6;
        };
    };
    uint8_t data[230];
} __attribute__((packed)) UpdateRequestPayload;

typedef struct
{
    uint32_t index;
    uint8_t nmr;
    uint8_t data[230];
} __attribute__((packed)) DataPayload;

typedef struct
{
    int32_t currentTime;
    char timezone[46];
    int32_t sunriseTime;
    int32_t sunsetTime;
} __attribute__((packed)) TimeSyncPayload;

typedef struct
{
    uint32_t sleepTime;
} __attribute__((packed)) SleepPayload;

typedef struct
{
    uint32_t max_mr_bytes;
    uint8_t type;
    DataPayload data;
} __attribute__((packed)) ByteStreamPayload;

typedef void (*DataReceivedCallback)(const uint8_t *mac_addr, const Message *incomingData, int len);
typedef void (*DataSentCallback)(const uint8_t *mac_addr, esp_now_send_status_t status);

class ESPNowCtrl
{
private:
    static QueueHandle_t sendQueue;
    static QueueHandle_t receiveQueue;
    static DataReceivedCallback onDataReceivedCallback;
    static DataSentCallback onDataSentCallback;

    static void onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

    static bool initDone;

public:
    static void Init();
    static void Deinit();

    static void SetDataReceivedCallback(DataReceivedCallback callback);
    static void SetDataSentCallback(DataSentCallback callback);

    static void SetChannel(uint8_t channel);

    template <typename Payload>
    static bool SendMessage(const uint8_t *peer_addr, uint8_t messageType, const Payload &payloadData, uint8_t payloadSize, uint8_t retryCount = 3)
    {
        return SendMessageInternal(peer_addr, messageType, (const uint8_t *)(&payloadData), payloadSize, retryCount);
    }
    static void SendMessageRaw(const uint8_t *peer_addr, uint8_t messageType, const uint8_t *payloadData, uint8_t payloadSize);
    static bool SendMessageInternal(const uint8_t *peer_addr, uint8_t messageType, const uint8_t *payload, uint8_t payloadSize, uint8_t retryCount);

    static bool SendMessage(const uint8_t *peer_addr, uint8_t messageType, uint8_t retryCount = 3);

    static void AddPeer(const uint8_t *mac_addr, uint8_t channel);
    static void DeletePeer(const uint8_t *mac_addr);
    static void Task(void);
    static void SetPower(wifi_power_t power);
};