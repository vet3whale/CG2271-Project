#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram_tx.h"
#include "shared_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart_tx.h"
#include "telegram_tx.h"
#include "passwords.h"

static WiFiClientSecure client;
static UniversalTelegramBot bot(BOT_TOKEN, client);

static void connectWiFiTele() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.println("connecting");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        client.setInsecure();
        Serial.println("connected");
        bot.sendMessage(CHAT_ID, "ESP32 online!", "");
    }
}


void Telegram_Init() {
    if (WiFi.status() == WL_CONNECTED) {
        client.setInsecure();
        Serial.println("connected");

        if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            int send = bot.sendMessage(CHAT_ID, "ESP32 online!", "");
            xSemaphoreGive(gNetworkMutex);
            Serial.print("[Telegram] Init send status: ");
            Serial.println(send);
        }
    }
}

void vTelegramTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        char geminiMsg[GEMINI_RESPONSE_MAX_LEN] = {0};
        Serial.println("[Telegram] Task running");

        if (xSemaphoreTake(gGeminiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            client.stop(); // Ensure previous session is dead
            client.setInsecure();
            if (gGeminiResponse[0] != '\0') {
                strncpy(geminiMsg, gGeminiResponse, GEMINI_RESPONSE_MAX_LEN - 1);
                geminiMsg[GEMINI_RESPONSE_MAX_LEN - 1] = '\0';
                gGeminiResponse[0] = '\0';
            }
            xSemaphoreGive(gGeminiMutex);
        }

        if (geminiMsg[0] != '\0') {
            if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                Telegram_Init();
                int send = bot.sendMessage(CHAT_ID, String(geminiMsg), "");
                
                xSemaphoreGive(gNetworkMutex);
                Serial.print("Send status: "); Serial.println(send);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}