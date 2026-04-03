#include <Arduino.h>
#include <WiFi.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "shared_data.h"
#include "dht_sensor.h"
#include "uart_rx.h"
#include "uart_tx.h"
#include "gemini.h"
#include "telegram_tx.h"
#include "oled_display.h"


SensorData_t      gSensorData  = {0};
SemaphoreHandle_t gSensorMutex = NULL;

QueueHandle_t gTelegramQueue = NULL;
SemaphoreHandle_t gGeminiMutex = NULL;
volatile bool gGeminiTrigger = false;

void WiFi_Connect(void) {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Already connected — skipping reinit");
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("connecting..");
    while (WiFi.status() != WL_CONNECTED) { delay(250); }
    Serial.println(WiFi.localIP());
}

SemaphoreHandle_t gNetworkMutex = NULL;

void setup() {
    Serial.begin(115200);
    gTelegramQueue = xQueueCreate(5, GEMINI_RESPONSE_MAX_LEN);
    gSensorMutex = xSemaphoreCreateMutex();
    gGeminiMutex = xSemaphoreCreateMutex();
    gNetworkMutex = xSemaphoreCreateMutex();
    DHT_Init();
    UART_RX_Init();
    UART_TX_Init();
    OLED_Init();


    WiFi_Connect();

    xTaskCreate(vOLEDTask, "OLED", OLED_TASK_STACK_SIZE, NULL, OLED_TASK_PRIORITY, NULL);
    xTaskCreate(vTelegramTask, "Telegram", 6144, NULL, 2, NULL); // Reduced from 8192
    xTaskCreate(vDHTTask,      "DHT",      2048, NULL, 2, NULL); 
    xTaskCreate(vUartRxTask,   "UartRX",   2048, NULL, 4, NULL); // Reduced from 4096
    xTaskCreate(vGeminiTask,   "Gemini",   6144, NULL, 2, NULL); // Reduced from 8192
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}