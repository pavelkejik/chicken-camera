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
#define EI_CLASSIFIER_TFLITE_ENABLE_PSRAM // arena in PSRAM
#include <Egg-counter_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

/* model input */
static constexpr int EI_W = EI_CLASSIFIER_INPUT_WIDTH;
static constexpr int EI_H = EI_CLASSIFIER_INPUT_HEIGHT;
static constexpr float CONF_THRESH = 0.6f;  // only show eggs ≥60% sure

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3

#define BOX_R 255
#define BOX_G 0
#define BOX_B 0

uint8_t *Camera::snapshot_buf = NULL;

/* buffer that will receive the annotated JPEG */
uint8_t *Camera::jpeg_out_buf = NULL;
size_t Camera::jpeg_out_size = 0;
uint8_t *Camera::full_rgb = NULL;

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
    .fb_location = CAMERA_FB_IN_PSRAM,
};

/* ------------ EI helper -------------------------------------------- */
int Camera::ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    size_t pixel_ix = offset * 3;
    size_t out_ptr_ix = 0;

    while (length--)
    {
        out_ptr[out_ptr_ix] =
            (snapshot_buf[pixel_ix + 2] << 16) +
            (snapshot_buf[pixel_ix + 1] << 8) +
            snapshot_buf[pixel_ix];

        out_ptr_ix++;
        pixel_ix += 3;
    }
    return 0;
}

/******** simple rectangle in RGB888 ********************/
void Camera::draw_rect_rgb888(
    uint8_t *buf, int img_w, int img_h,
    int x0, int y0, int w, int h,
    uint8_t r, uint8_t g, uint8_t b)
{
    const int thickness = 2;

    // compute inner & outer bounds
    int x1 = std::max(0, x0);
    int y1 = std::max(0, y0);
    int x2 = std::min(img_w - 1, x0 + w - 1);
    int y2 = std::min(img_h - 1, y0 + h - 1);

    // draw top and bottom bands
    for (int t = 0; t < thickness; ++t) {
        int yt = y1 + t;     // line t below top
        int yb = y2 - t;     // line t above bottom

        if (yt >= 0 && yt < img_h) {
            for (int x = x1; x <= x2; ++x) {
                size_t idx = (yt * img_w + x) * 3;
                buf[idx    ] = r;
                buf[idx + 1] = g;
                buf[idx + 2] = b;
            }
        }
        if (yb >= 0 && yb < img_h) {
            for (int x = x1; x <= x2; ++x) {
                size_t idx = (yb * img_w + x) * 3;
                buf[idx    ] = r;
                buf[idx + 1] = g;
                buf[idx + 2] = b;
            }
        }
    }

    // draw left and right bands
    for (int t = 0; t < thickness; ++t) {
        int xl = x1 + t;     // line t right of left edge
        int xr = x2 - t;     // line t left of right edge

        if (xl >= 0 && xl < img_w) {
            for (int y = y1; y <= y2; ++y) {
                size_t idx = (y * img_w + xl) * 3;
                buf[idx    ] = r;
                buf[idx + 1] = g;
                buf[idx + 2] = b;
            }
        }
        if (xr >= 0 && xr < img_w) {
            for (int y = y1; y <= y2; ++y) {
                size_t idx = (y * img_w + xr) * 3;
                buf[idx    ] = r;
                buf[idx + 1] = g;
                buf[idx + 2] = b;
            }
        }
    }
}

void Camera::run_edge_impulse(camera_fb_t *fb)
{
    /******** 1. decode JPEG -> snapshot_buf (320x240 RGB888) ********/
    if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf))
    {
        Serial.println("JPEG decode failed");
        return;
    }

    /* Make a *copy* of the full-res frame before we resize over it. */
    size_t fullBytes = EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE;
    memcpy(full_rgb, snapshot_buf, fullBytes);

    /******** 2. resize copy in-place down to model size ********/
    ei::image::processing::crop_and_interpolate_rgb888(
        snapshot_buf,
        EI_CAMERA_RAW_FRAME_BUFFER_COLS, EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
        snapshot_buf,
        EI_W, EI_H);

    /******** 3. build signal & run classifier ********/
    ei::signal_t signal{
        .get_data = [](size_t off, size_t len, float *dst) -> int
        {
            size_t ix = off * 3;
            for (size_t i = 0; i < len; ++i, ix += 3)
            {
                dst[i] = ((uint32_t)snapshot_buf[ix + 2] << 16) |
                         ((uint32_t)snapshot_buf[ix + 1] << 8) |
                         (uint32_t)snapshot_buf[ix];
            }
            return 0;
        },
        .total_length = EI_W * EI_H

    };

    ei_impulse_result_t res{};
    if (run_classifier(&signal, &res, false) != EI_IMPULSE_OK)
    {
        Serial.println("EI run failed");
        return;
    }

    /******** 4. draw boxes back on the *full* image ********/
    const float sx = (float)EI_CAMERA_RAW_FRAME_BUFFER_COLS / EI_W;
    const float sy = (float)EI_CAMERA_RAW_FRAME_BUFFER_ROWS / EI_H;

        int   count = 0;
    for (size_t i = 0; i < res.bounding_boxes_count; ++i)
    {
        auto &bb = res.bounding_boxes[i];
        if (bb.value < CONF_THRESH) continue;      // skip low-confidence
        ++count;
        int x = (int)(bb.x * sx);
        int y = (int)(bb.y * sy);
        int w = (int)(bb.width * sx);
        int h = (int)(bb.height * sy);

        draw_rect_rgb888(full_rgb,
                         EI_CAMERA_RAW_FRAME_BUFFER_COLS,
                         EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
                         x, y, w, h, 255, 0, 0);
    }

    if(PocetVajec.Get() != count)
    {
        SystemLog::PutLog("Pocet vajicek se zmenil z " + String(PocetVajec.Get()) + " na " + String(count), v_info);
    }
    PocetVajec.Set(count);

    /******** 5. encode RGB888 -> JPEG  **********************/
    uint8_t *jpeg_buf = nullptr;
    size_t jpeg_len = 0;

    bool enc_ok = fmt2jpg(full_rgb,
                          EI_CAMERA_RAW_FRAME_BUFFER_COLS *
                              EI_CAMERA_RAW_FRAME_BUFFER_ROWS * 3,
                          EI_CAMERA_RAW_FRAME_BUFFER_COLS,
                          EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
                          PIXFORMAT_RGB888,
                          80, // quality 0-100
                          &jpeg_buf, &jpeg_len);
    if (!enc_ok)
    {
        Serial.println("JPEG encode failed");
        return;
    }

    /******** 6. swap the buffers so ESP-NOW sends annotated frame ****/
    esp_camera_fb_return(fb);      // give back original
    picture = esp_camera_fb_get(); // take a *new* empty fb
    if (!picture)
    { // extremely unlikely
        Serial.println("fb_get failed after return");
        free(jpeg_buf);
        return;
    }
    /* replace its buffer pointer with our JPEG */
    picture->buf = jpeg_buf;
    picture->len = jpeg_len;
    picture->format = PIXFORMAT_JPEG;
    picture->width = EI_CAMERA_RAW_FRAME_BUFFER_COLS;
    picture->height = EI_CAMERA_RAW_FRAME_BUFFER_ROWS;

    /******** 7. log ********/
    Serial.printf("eggs=%u  (DSP:%d ms NN:%d ms)  JPEG:%u bytes\n",
                  res.bounding_boxes_count,
                  res.timing.dsp,
                  res.timing.classification,
                  (unsigned)jpeg_len);
}

void Camera::Init()
{
    if (!psramInit())
    {
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
    s->set_hmirror(s, 1);
    s->set_vflip  (s, 1);

    if (!snapshot_buf)
    {
        snapshot_buf = (uint8_t *)ps_malloc(
            EI_CAMERA_RAW_FRAME_BUFFER_COLS *
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS *
            EI_CAMERA_FRAME_BYTE_SIZE);
        if (!snapshot_buf)
        {
            Serial.println("PSRAM alloc failed");
        }
    }

    if (!full_rgb)
    {
        size_t fullSize = EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE;
        full_rgb = (uint8_t *)ps_malloc(
            fullSize);
        if (!full_rgb)
        {
            Serial.println("PSRAM alloc failed for full_rgb");
        }
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
    if (!picture)
    {
        Serial.println("Capture failed");
        return;
    }

    if (snapshot_buf)
    {
        run_edge_impulse(picture); // Use renamed function
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