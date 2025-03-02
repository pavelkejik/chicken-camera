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