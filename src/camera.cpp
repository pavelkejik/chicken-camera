/***********************************************************************
 * Filename: camera.cpp
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Implements the Camera class.
 *
 ***********************************************************************/

#include "camera.h"
#include "esp_heap_caps.h"                  
#include "pin_map.h"
#include "esp_camera.h"
#include "esp_now_ctrl.h"
#include "parameters.h"
#include "log.h"
#include "esp_now_client.h"
#include "deep_sleep_ctrl.h"

/* ---- Edge Impulse -------------------------------------------------- */
#define EI_CLASSIFIER_TFLITE_ENABLE_PSRAM     // arena in PSRAM
#include <Egg-counter_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

/* model input */
constexpr int EI_W = EI_CLASSIFIER_INPUT_WIDTH; 
constexpr int EI_H = EI_CLASSIFIER_INPUT_HEIGHT;

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

static uint8_t *snapshot_buf = nullptr;

/* -------------------------------------------------------------------- */



camera_fb_t *Camera::picture = NULL;
SemaphoreHandle_t Camera::semaphore = xSemaphoreCreateBinary();

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, // YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,   // QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 10, // 0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,      // When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    // .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location  = CAMERA_FB_IN_PSRAM, 
};

/* ------------ EI helper -------------------------------------------- */
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    size_t pixel_ix = offset * 3;
    size_t out_ptr_ix = 0;

    while (length--) {
        out_ptr[out_ptr_ix] = 
            (snapshot_buf[pixel_ix + 2] << 16) + 
            (snapshot_buf[pixel_ix + 1] << 8) + 
            snapshot_buf[pixel_ix];
        
        out_ptr_ix++;
        pixel_ix += 3;
    }
    return 0;
}

static void run_edge_impulse(camera_fb_t *fb)
{
    /* JPEG → RGB888 (into PSRAM buffer) */
    if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf)) {
        Serial.println("JPEG decode failed");
        return;
    }

    // Resize if needed
    if (EI_CLASSIFIER_INPUT_WIDTH != EI_CAMERA_RAW_FRAME_BUFFER_COLS || 
        EI_CLASSIFIER_INPUT_HEIGHT != EI_CAMERA_RAW_FRAME_BUFFER_ROWS) {
        
        ei::image::processing::crop_and_interpolate_rgb888(
            snapshot_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            snapshot_buf,
            EI_CLASSIFIER_INPUT_WIDTH,
            EI_CLASSIFIER_INPUT_HEIGHT);
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    // Run inference
    ei_impulse_result_t res;
    EI_IMPULSE_ERROR err = run_classifier(&signal, &res, false);

    Serial.printf("eggs=%u  (DSP:%d ms NN:%d ms)\n",
                  res.bounding_boxes_count,
                  res.timing.dsp, res.timing.classification);

    for (size_t i = 0; i < res.bounding_boxes_count; ++i) {
        auto &bb = res.bounding_boxes[i];
        if (!bb.value) continue;
        Serial.printf("  #%u %.2f  [x:%u y:%u w:%u h:%u]\n",
                      i, bb.value, bb.x, bb.y, bb.width, bb.height);
    }
}


void Camera::Init()
{
    if(!psramInit()) {
        Serial.println("PSRAM not available");
    }
    picture = NULL;
    SetTimezone(PopisCasu.Get().c_str());
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        Serial.printf("Camera init failed with error 0x%x", err);
    }
    pinMode(FLASH_PIN, OUTPUT);
    sensor_t *s = esp_camera_sensor_get();
    s->set_gain_ctrl(s, 1);     // auto gain on
    s->set_exposure_ctrl(s, 1); // auto exposure on
    s->set_awb_gain(s, 1);      // Auto White Balance enable (0 or 1)
    s->set_brightness(s, 1);    // (-2 to 2) - set brightness

        if (!snapshot_buf) {
        snapshot_buf = (uint8_t*)ps_malloc(
            EI_CAMERA_RAW_FRAME_BUFFER_COLS * 
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS * 
            EI_CAMERA_FRAME_BYTE_SIZE);
        if (!snapshot_buf) {
            Serial.println("PSRAM alloc failed");        }
    }
}

bool Camera::IsDay(void)
{
    time_t currentTime = Now();
    time_t sunriseTime = CasVychodu.Get();
    time_t sunsetTime = CasZapadu.Get();
    if (sunriseTime == 0 && sunsetTime == 0)
    {
        return false;
    }

    time_t adjustedSunriseTime = sunriseTime + PosunVychodu.Get() * 60;
    time_t adjustedSunsetTime = sunsetTime + PosunZapadu.Get() * 60;

    if (adjustedSunriseTime < currentTime && adjustedSunsetTime < currentTime && sunriseTime != 0 && sunsetTime != 0)
    {
        if (adjustedSunriseTime < adjustedSunsetTime)
        {
            CasVychodu.Set(sunriseTime + 24 * 60 * 60);
            adjustedSunriseTime += 24 * 60 * 60;
        }
        else
        {
            CasZapadu.Set(sunsetTime + 24 * 60 * 60);
            adjustedSunsetTime += 24 * 60 * 60;
        }
    }

    if (adjustedSunriseTime < currentTime && adjustedSunsetTime < currentTime)
    {
        if (adjustedSunriseTime > adjustedSunsetTime)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((adjustedSunriseTime < currentTime) && sunriseTime != 0)
    {
        return true;
    }
    else if ((adjustedSunsetTime < currentTime) && sunsetTime != 0)
    {
        return false;
    }
    else
    {
        return false;
    }
}

void Camera::Task(void)
{
    if (xSemaphoreTake(semaphore, pdMS_TO_TICKS(300)) == pdTRUE)
    {
        active_tasks[Camera_Task] = true;

        if (picture == NULL)
        {
            switch (KonfiguraceSnimani.Get())
            {
            case automaticky:
                if (PoriditSnimek.Get() || IsDay())
                {
                    TakePicture();
                }
                break;

            case vzdy:
                TakePicture();
                break;

            case nikdy:
                if (PoriditSnimek.Get())
                {
                    TakePicture();
                }
                break;

            default:
                break;
            }
        }
    }
    active_tasks[Camera_Task] = false;
}

bool Camera::SendPictureViaEspNow(const uint8_t *mac_addr)
{
    Serial.println("Sending photo");
    unsigned int count = picture->len;

    size_t cnv_buf_len = picture->len;
    uint8_t *cnv_buf = picture->buf;
    // bool isConverted = frame2jpg(picture, 80, &cnv_buf, &cnv_buf_len);
    // if (!isConverted)
    // {
    //     Serial.println("failed to convert");
    // }

    if (picture == NULL || cnv_buf_len == 0 || cnv_buf == NULL)
    {
        return true;
    }
    bool sendMessageSuccess = true;

    ByteStreamPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.max_mr_bytes = cnv_buf_len;

    size_t currentIndex = 0;
    size_t payload_cap = sizeof(DataPayload::data);
    size_t payload_size = sizeof(payload) - payload_cap;

    while (currentIndex < cnv_buf_len)
    {
        memset(&payload.data, 0, sizeof(payload.data));
        payload.data.index = currentIndex;

        size_t bytesLeft = cnv_buf_len - currentIndex;
        size_t bytesToCopy = bytesLeft < payload_cap ? bytesLeft : payload_cap;

        memcpy(payload.data.data, cnv_buf + currentIndex, bytesToCopy);
        payload.data.nmr = bytesToCopy;

        CHECK_SEND_RETURN_IF_FAIL(ESPNowCtrl::SendMessage(mac_addr, MSG_BYTE_STREAM, payload, payload_size + bytesToCopy, 5));

        currentIndex += bytesToCopy;
    }
    Serial.println("Picture sent");

    return sendMessageSuccess;
}

void Camera::TakePicture()
{
    switch (PouzitBlesk.Get())
    {
    case automaticky:
    {
        if (!IsDay())
        {
            digitalWrite(FLASH_PIN, HIGH);
            delay(500);
        }
        break;
    }
    case vzdy:
        digitalWrite(FLASH_PIN, HIGH);
        delay(500);

        break;
    case nikdy:
        digitalWrite(FLASH_PIN, LOW);
        break;
    default:
        break;
    }

    picture = esp_camera_fb_get();
    if (!picture) {
        Serial.println("Capture failed");
        return;
    }

    if (snapshot_buf) {
        run_edge_impulse(picture);  // Use renamed function
    }

    digitalWrite(FLASH_PIN, LOW);

    Serial.printf("Picture taken! Its size was: %zu bytes\n", picture->len);
    ESPNowClient::SendPhoto();
}

void Camera::DeletePicture()
{
    esp_camera_fb_return(picture);
}

void Camera::Wake(void)
{
    active_tasks[Camera_Task] = true;
    xSemaphoreGive(semaphore);
}