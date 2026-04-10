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

void vTelegramTask(void *pvParameters) {
    char rxBuffer[GEMINI_RESPONSE_MAX_LEN];

    while (1) {
        // block until a message is added to the queue by gemini
        if (xQueueReceive(gTelegramQueue, &rxBuffer, portMAX_DELAY) == pdPASS) {
            Serial.println("[Telegram] New message dequeued");

            // take network mutex to send
            if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(10000)) == pdTRUE) {
                client.setInsecure();
                bot.sendMessage(CHAT_ID, String(rxBuffer), "");
                xSemaphoreGive(gNetworkMutex);
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}