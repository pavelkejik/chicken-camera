/***********************************************************************
 * Filename: camera.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Declares the Camera class, which provides methods for initializing
 *     the camera, taking and deleting pictures, sending pictures via
 *     ESP-NOW, and checking lighting conditions.
 ***********************************************************************/

#pragma once

#include "Arduino.h"
#include "esp_camera.h"

class Camera
{
private:
    static SemaphoreHandle_t semaphore;
    static uint8_t *snapshot_buf;
    static uint8_t *jpeg_out_buf;
    static size_t jpeg_out_size;
    static uint8_t *full_rgb;

    static void run_edge_impulse(camera_fb_t *fb);
    static void draw_rect_rgb888(
        uint8_t *buf, int img_w, int img_h,
        int x0, int y0, int w, int h,
        uint8_t r = 255, uint8_t g = 0, uint8_t b = 0);
    static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);

public:
    static camera_fb_t *picture;

    static void Init();
    static void TakePicture();
    static void DeletePicture();
    static bool SendPictureViaEspNow(const uint8_t *mac_addr);
    static void Task(void);
    static void Wake(void);
    static bool IsDay(void);
};