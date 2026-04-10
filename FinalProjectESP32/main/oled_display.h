#pragma once
#include <Arduino.h>

void OLED_Init();
void vOLEDTask(void *pvParameters);

#define OLED_TASK_STACK_SIZE 4096
#define OLED_TASK_PRIORITY 1
