#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram_tx.h"
#include "shared_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "passwords.h"

static WiFiClientSecure client;
static UniversalTelegramBot bot(BOT_TOKEN, client);

#define TELEGRAM_COOLDOWN_MS    15000
#define PERSONALITY_TIMEOUT_MS  15000
#define PERSONALITY_POLL_MS       500

static unsigned long sLastTelegramSend = 0;

/* ── Personalities stored in flash ─────────────────────────────────────── */
static const char P_FUNNY[] PROGMEM =
    "You are a funny, lovable study buddy who speaks in casual Singlish-English. "
    "Use Singlish: lah, leh, sia, walao, aiyah. Warm, teasing, dramatic tone. "
    "Celebrate wins, react badly to poor conditions. Always give one practical suggestion. "
    "Never be vulgar or harsh. Keep it light and encouraging.";

static const char P_STRICT[] PROGMEM =
    "You are a strict, no-nonsense study coach. Short, direct sentences. No sugarcoating. "
    "High standards, blunt but not cruel. No humor, no filler, no small talk. "
    "Call out poor conditions, demand corrective action. End with one firm directive.";

static const char P_FORMAL[] PROGMEM =
    "You are a professional academic advisor giving a structured post-session debrief. "
    "Formal, precise language. Analytical tone like a written report. "
    "Assess conditions objectively. End with one well-reasoned professional recommendation.";

static const char P_ZEN[] PROGMEM =
    "You are a calm, mindful wellness coach. Soft, unhurried, grounding tone. "
    "Reframe poor conditions as growth opportunities. Peaceful language, no harsh words. "
    "Surface one key insight gently. End with one calming, practical recommendation.";

static const char* sPersonality = P_FUNNY;

String Telegram_GetPersonality() {
    return String(sPersonality);
}

static const char KEYBOARD_JSON[] PROGMEM =
    "[[{\"text\":\"Funny\",\"callback_data\":\"/funny\"},"
     "{\"text\":\"Strict\",\"callback_data\":\"/strict\"}],"
     "[{\"text\":\"Formal\",\"callback_data\":\"/formal\"},"
     "{\"text\":\"Zen\",\"callback_data\":\"/zen\"}]]";

static void checkForCommands() {
    if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
        Serial.println("[Telegram] Mutex timeout on startup.");
        return;
    }

    client.setInsecure();
    bot.sendMessageWithInlineKeyboard(
        CHAT_ID,
        "ESP32 restarted! Choose your study coach personality:",
        "",
        String(FPSTR(KEYBOARD_JSON))
    );

    while (bot.getUpdates(bot.last_message_received + 1) > 0) {}
    xSemaphoreGive(gNetworkMutex);

    Serial.println("[Telegram] Menu sent. Waiting 15 s...");

    unsigned long start = millis();
    bool selected = false;

    while (!selected && (millis() - start < PERSONALITY_TIMEOUT_MS)) {
        vTaskDelay(pdMS_TO_TICKS(PERSONALITY_POLL_MS));

        if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
            continue;
        }

        client.setInsecure();
        int n = bot.getUpdates(bot.last_message_received + 1);

        for (int i = 0; i < n && !selected; i++) {
            String text    = bot.messages[i].text;
            String chat    = bot.messages[i].chat_id;
            String queryId = bot.messages[i].query_id;
            bool isCallback = (bot.messages[i].type == "callback_query");

            if (isCallback) {
                bot.answerCallbackQuery(queryId, "");
            }

            if (text == "/funny") {
                sPersonality = P_FUNNY;
                bot.sendMessage(chat, "Funny mode activated!", "");
                selected = true;
            } else if (text == "/strict") {
                sPersonality = P_STRICT;
                bot.sendMessage(chat, "Strict mode activated.", "");
                selected = true;
            } else if (text == "/formal") {
                sPersonality = P_FORMAL;
                bot.sendMessage(chat, "Formal mode activated.", "");
                selected = true;
            } else if (text == "/zen") {
                sPersonality = P_ZEN;
                bot.sendMessage(chat, "Zen mode activated.", "");
                selected = true;
            }
        }

        xSemaphoreGive(gNetworkMutex);
    }

    if (!selected) {
        sPersonality = P_FUNNY;
        Serial.println("[Telegram] Timed out. Defaulting to FUNNY.");

        if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            client.setInsecure();
            bot.sendMessage(CHAT_ID, "No reply - defaulting to Funny mode.", "");
            xSemaphoreGive(gNetworkMutex);
        }
    }
}

void vTelegramTask(void *pvParameters) {
    static char rxBuffer[GEMINI_RESPONSE_MAX_LEN];

    checkForCommands();

    while (1) {
        if (xQueueReceive(gTelegramQueue, rxBuffer, portMAX_DELAY) == pdPASS) {
            unsigned long now = millis();

            if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(10000)) == pdTRUE) {
                client.setInsecure();

                if (sLastTelegramSend != 0 &&
                    (now - sLastTelegramSend) < TELEGRAM_COOLDOWN_MS) {
                    Serial.println("[Telegram] Cooldown - dropping message.");
                    xSemaphoreGive(gNetworkMutex);
                    continue;
                }

                if (bot.sendMessage(CHAT_ID, String(rxBuffer), "")) {
                    Serial.println("[Telegram] Send success");
                } else {
                    Serial.println("[Telegram] Send failed");
                }

                sLastTelegramSend = millis();
                xSemaphoreGive(gNetworkMutex);
            }
        }
    }
}