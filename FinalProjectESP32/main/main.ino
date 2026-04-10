#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

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

SensorData_t gSensorData = { 0 };
SemaphoreHandle_t gSensorMutex = NULL;
volatile bool gSystemReady = false;

QueueHandle_t gTelegramQueue = NULL;
SemaphoreHandle_t gGeminiMutex = NULL;
SemaphoreHandle_t gGeminiSemaphore = NULL;

char gGeminiOLEDMsg[GEMINI_RESPONSE_MAX_LEN] = { 0 };
volatile bool gGeminiOLEDReady = false;

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
  gTelegramQueue = xQueueCreate(1, GEMINI_RESPONSE_MAX_LEN);
  gSensorMutex = xSemaphoreCreateMutex();
  gGeminiMutex = xSemaphoreCreateMutex();
  gNetworkMutex = xSemaphoreCreateMutex();
  gGeminiSemaphore = xSemaphoreCreateBinary();

  DHT_Init();
  UART_RX_Init();
  UART_TX_Init();
  OLED_Init();

  WiFi_Connect();
  gSystemReady = true;

  xTaskCreate(vOLEDTask, "OLED", OLED_TASK_STACK_SIZE, NULL, OLED_TASK_PRIORITY, NULL);
  xTaskCreate(vTelegramTask, "Telegram", 6144, NULL, 3, NULL);
  xTaskCreate(vDHTTask, "DHT", 2048, NULL, 2, NULL);
  xTaskCreate(vUartRxTask, "UartRX", 2048, NULL, 4, NULL);
  xTaskCreate(vGeminiTask, "Gemini", 6144, NULL, 2, NULL);
}

unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL_MS = 120000;  // 2 minutes

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi_Connect();
  }

  if (millis() - lastPingTime > PING_INTERVAL_MS) {
    lastPingTime = millis();
    if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
      HTTPClient http;
      http.begin("http://clients3.google.com/generate_204");
      int httpCode = http.GET();
      http.end();
      xSemaphoreGive(gNetworkMutex);
    }
  }
}