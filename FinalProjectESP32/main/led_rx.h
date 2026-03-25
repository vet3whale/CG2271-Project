#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern HardwareSerial Serial2;  // DECLARATION — tells other files it exists

void LED_RX_Init();
void vSerialRxTask(void *pvParameters);
void vLEDTask(void *pvParameters);