#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>

typedef struct {
	uint8_t tap_event;
	uint8_t on_off;
	uint8_t paused;
    uint16_t light_raw;
    uint16_t sound_raw;
    uint8_t  sound_triggered;
    int8_t   temperature;
    uint8_t  temp_frac;
    uint8_t  env_condition;
} SensorData_t;


extern SemaphoreHandle_t gSensorMutex;
extern SemaphoreHandle_t gADCMutex;
extern SemaphoreHandle_t gTempReadySem;

extern volatile SensorData_t gSensorData;
extern volatile SensorData_t gAverageSensorData;

extern volatile int led_state;
extern volatile uint8_t g_color_blend; // 0 = Green, 100 = Red

extern TaskHandle_t xTxTaskHandle; // used if u wanna immediately tx data

void vPrintTask(void *pvParameters);

#endif
