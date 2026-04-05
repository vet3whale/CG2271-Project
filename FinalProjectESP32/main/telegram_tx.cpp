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

/* ── Personality ─────────────────────────────────────────────────────────── */
static String sPersonality =
    "You are a funny study buddy. "
    "You reply in casual Singlish-English, playful and teasing, like a close friend roasting the user a bit. "
    "Be dramatic and funny, but still helpful. "
    "Do not be vulgar, hateful, or overly harsh. "
    "Always include one practical suggestion.";

String Telegram_GetPersonality() {
    return sPersonality;
}

static void checkForCommands() {
    int numNew = bot.getUpdates(bot.last_message_received + 1);
    for (int i = 0; i < numNew; i++) {
        String text = bot.messages[i].text;
        String chat = bot.messages[i].chat_id;

        if (text == "/start" || text == "/personality") {
            String keyboardJson =
                "[[{\"text\":\"😂 Funny\",\"callback_data\":\"/funny\"},"
                "{\"text\":\"😤 Strict\",\"callback_data\":\"/strict\"}],"
                "[{\"text\":\"💼 Formal\",\"callback_data\":\"/formal\"},"
                "{\"text\":\"🧘 Zen\",\"callback_data\":\"/zen\"}]]";

            bot.sendMessageWithInlineKeyboard(chat,
                "Choose your study coach personality:",
                "",
                keyboardJson);

        } else if (text == "/funny") {
            sPersonality =
                "You are a funny study buddy. "
                "You reply in casual Singlish-English, playful and teasing, like a close friend roasting the user a bit. "
                "Be dramatic and funny, but still helpful. "
                "Do not be vulgar, hateful, or overly harsh. "
                "Always include one practical suggestion.";
            bot.sendMessage(chat, "😂 Funny Singlish mode activated lah!", "");

        } else if (text == "/strict") {
            sPersonality =
                "You are a strict no-nonsense study coach. "
                "Be direct and blunt. Call out bad habits. "
                "No jokes. Push the user to do better. "
                "Always include one practical suggestion.";
            bot.sendMessage(chat, "😤 Strict mode activated. No excuses.", "");

        } else if (text == "/formal") {
            sPersonality =
                "You are a professional academic advisor. "
                "Be concise, precise and formal. "
                "Always include one practical suggestion.";
            bot.sendMessage(chat, "💼 Formal mode activated.", "");

        } else if (text == "/zen") {
            sPersonality =
                "You are a calm, mindful wellness coach. "
                "Use peaceful, gentle language. Focus on balance and wellbeing. "
                "Always include one practical suggestion.";
            bot.sendMessage(chat, "🧘 Zen mode activated.", "");
        }
    }
}

/* ── Task — unchanged except checkForCommands() added at top of loop ─────── */
void vTelegramTask(void *pvParameters) {
    char rxBuffer[GEMINI_RESPONSE_MAX_LEN];

    while (1) {
        // Poll for personality commands
        checkForCommands();

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