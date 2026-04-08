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

#define TELEGRAM_COOLDOWN_MS 15000
static unsigned long sLastTelegramSend = 0;

void vTelegramTask(void *pvParameters) {
    char rxBuffer[GEMINI_RESPONSE_MAX_LEN];

    while (1) {
        // This will wait (block) indefinitely until a message is added to the queue
        if (xQueueReceive(gTelegramQueue, &rxBuffer, portMAX_DELAY) == pdPASS) {
            Serial.println("[Telegram] New message dequeued");
            unsigned long now = millis();

            // Take network mutex to send
            if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(10000)) == pdTRUE) {
                client.setInsecure();
                if (sLastTelegramSend != 0 && (now - sLastTelegramSend) < TELEGRAM_COOLDOWN_MS) {
                    Serial.println("[Telegram] BLOCKED: Cooldown active. Dropping duplicate message.");
                    xSemaphoreGive(gNetworkMutex);
                    continue;
                }                  

                if (!(sLastTelegramSend && (now - sLastTelegramSend) < TELEGRAM_COOLDOWN_MS) && bot.sendMessage(CHAT_ID, String(rxBuffer), "")) {
                    Serial.println("[Telegram] Send success");
                } else {
                    Serial.println("[Telegram] Send failed");
                }
                sLastTelegramSend = millis();
                xSemaphoreGive(gNetworkMutex);
            }
            
            // Short rest to let the WiFi hardware stabilize
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}