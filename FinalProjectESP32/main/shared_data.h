#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <stdint.h>

typedef struct {
    /* DHT11 — written by vDHTTask */
    float   esp_temp;
    float   esp_humidity;
    uint8_t led_status;         // 0=green, 1=amber, 2=red

    /* UART — written by vUartRxTask (from MCXC444) */
    uint8_t  tap_event;         // 1 = tap detected
    uint8_t  on_off;        // 1 = focus mode active
    uint8_t  paused;
    uint16_t light_raw;         // ADC 0-1023
    uint16_t sound_raw;         // ADC 0-1023
    uint8_t  sound_triggered;   // 1 = sound event this cycle
    uint8_t  env_condition;
    uint8_t  temp;
    uint8_t  temp_frac;
    uint8_t  hum;
    uint8_t  hum_frac;
    uint32_t session_duration_sec;
} SensorData_t;

extern SensorData_t      gSensorData;
extern SemaphoreHandle_t gSensorMutex;

#define GEMINI_RESPONSE_MAX_LEN 512

extern QueueHandle_t gTelegramQueue;
extern SemaphoreHandle_t gGeminiMutex;
extern SemaphoreHandle_t gGeminiSemaphore;

extern SemaphoreHandle_t gNetworkMutex;
extern volatile bool gSystemReady;

extern char gGeminiOLEDMsg[GEMINI_RESPONSE_MAX_LEN];
extern volatile bool gGeminiOLEDReady;

#endif // SHARED_DATA_H