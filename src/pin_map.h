#pragma once
/***********************************************************************
 * Pin map – ESP32 AI-Thinker vs. ESP-EYE
 * Přepínej odkomentováním jednoho z definů níže (nebo
 *   přidej -D CAMERA_MODEL_… do build_flags v platformio.ini).
 ***********************************************************************/

//#define CAMERA_MODEL_AI_THINKER   // ESP32-CAM (AI-Thinker)
#define CAMERA_MODEL_ESP_EYE       // ESP-EYE v2.1

/* ------------------------------------------------------------------ */
#if defined(CAMERA_MODEL_AI_THINKER)
/* AI-Thinker ESP32-CAM **********************************************/
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22
#define FLASH_PIN        4        // integrovaná bílá LED

/* ------------------------------------------------------------------ */
#elif defined(CAMERA_MODEL_ESP_EYE)
/* Espressif ESP-EYE ***********************************************/
/* oficiální mapování z Arduino-ESP32 → camera_pins.h */
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK     4
#define CAM_PIN_SIOD    18
#define CAM_PIN_SIOC    23
#define CAM_PIN_D7      36
#define CAM_PIN_D6      37
#define CAM_PIN_D5      38
#define CAM_PIN_D4      39
#define CAM_PIN_D3      35
#define CAM_PIN_D2      14
#define CAM_PIN_D1      13
#define CAM_PIN_D0      34
#define CAM_PIN_VSYNC    5
#define CAM_PIN_HREF    27
#define CAM_PIN_PCLK    25
#define FLASH_PIN       -1        // LED sdílí XCLK → blesk vypneme

/* ------------------------------------------------------------------ */
#else
  #error "Vyber CAM desku: CAMERA_MODEL_AI_THINKER nebo CAMERA_MODEL_ESP_EYE"
#endif
