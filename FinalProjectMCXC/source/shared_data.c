#include "shared_data.h"
#include "led.h"
#include "uart_packet.h"

#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"

SemaphoreHandle_t gSensorMutex = NULL;
SemaphoreHandle_t gADCMutex = NULL;
SemaphoreHandle_t gTempReadySem = NULL;

volatile SensorData_t gSensorData = { 0 };
volatile SensorData_t gAverageSensorData = { 0 };
volatile int led_state = 0;
volatile uint8_t g_color_blend = 0;
volatile uint16_t soundTriggerCount30s = 0;

TaskHandle_t xTxTaskHandle = NULL;

