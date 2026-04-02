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

/* ── Shared handles ──────────────────────────────────────────────────────── */
SensorData_t      gSensorData  = {0};
SemaphoreHandle_t gSensorMutex = NULL;

/* ── Monitor task ────────────────────────────────────────────────────────── */
void vMonitorTask(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void WiFi_Connect(void) {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Already connected — skipping reinit");
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) { delay(250); }
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(115200);
    delay(3000);

    gSensorMutex = xSemaphoreCreateMutex();
    DHT_Init();

    UART_RX_Init();
    UART_TX_Init();

    WiFi_Connect();
    Telegram_Init();
    Gemini_Init();

    xTaskCreate(vTelegramTask, "Telegram", TELEGRAM_TASK_STACK_SIZE, NULL, TELEGRAM_TASK_PRIORITY, NULL);

    xTaskCreate(vDHTTask,      "DHT",      DHT_TASK_STACK_SIZE, NULL, TELEGRAM_TASK_PRIORITY, NULL);
    xTaskCreate(vMonitorTask,  "Monitor",  2048,                NULL, TELEGRAM_TASK_PRIORITY,                 NULL);

    xTaskCreate(vUartRxTask,   "UartRX",   4096,                NULL, 4,                 NULL);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}