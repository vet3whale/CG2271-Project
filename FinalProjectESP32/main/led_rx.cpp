#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_rx.h"

HardwareSerial Serial2(2);

#define LED_PIN         26
#define UART_RX_PIN     16
#define UART_BAUD       115200
#define DATA_TIMEOUT_MS 200

static volatile TickType_t gLastRxTick = 0;

void vSerialRxTask(void *pvParameters) {
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(3000));
    while (Serial2.available() > 0) Serial2.read();  // flush boot noise
    while (1) {
        if (Serial2.available() > 0) {
            while (Serial2.available() > 0) Serial2.read();
            gLastRxTick = xTaskGetTickCount();
            Serial.println("Data received");
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void vLEDTask(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        TickType_t now     = xTaskGetTickCount();
        TickType_t elapsed = (now - gLastRxTick) * portTICK_PERIOD_MS;
        if (gLastRxTick != 0 && elapsed < DATA_TIMEOUT_MS) {
            digitalWrite(LED_PIN, HIGH);
        } else {
            if (digitalRead(LED_PIN) == HIGH) Serial.println("Data stopped");
            digitalWrite(LED_PIN, LOW);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void LED_RX_Init() {
    Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, -1);
}
