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

/* ── Timing ──────────────────────────────────────────────────────────────── */
#define BOT_POLL_MS           3000
#define TELEGRAM_COOLDOWN_MS  15000

static unsigned long sLastTelegramSend = 0;
static unsigned long sLastPollTime     = 0;

/* ── Personality ─────────────────────────────────────────────────────────── */
static volatile bool sPersonalitySet = false;

static String sPersonality =
    "You are a funny, lovable study buddy who speaks in casual Singlish-English. "
    "Your tone is warm, teasing, and dramatic — like a close friend who roasts the user just enough to make them laugh but never feels mean. "
    "You use Singlish expressions naturally: 'lah', 'leh', 'sia', 'walao', 'confirm plus chop', 'die die must', 'aiyah', etc. "
    "You celebrate small wins dramatically and react to bad conditions like it is a personal tragedy. "
    "You are still genuinely helpful — always include exactly one clear, practical suggestion. "
    "Never be vulgar, hateful, or overly harsh. "
    "Keep the tone light, fun, and encouraging overall.";

static const String PERSONALITY_FORMAL =
    "You are a professional academic advisor providing a structured post-session debrief. "
    "Your language is formal, composed, and precise — no slang, contractions, or casual expressions. "
    "You present the study environment data in a measured, analytical tone, similar to a written academic report. "
    "Acknowledge the user's effort professionally, then provide a clear, evidence-based assessment of the study conditions. "
    "Highlight the most significant environmental factor affecting study quality. "
    "Conclude with exactly one well-reasoned, actionable recommendation phrased as professional advice. "
    "Maintain a respectful, objective tone throughout.";

String Telegram_GetPersonality() { return sPersonality; }
bool   Telegram_IsPersonalitySet() { return sPersonalitySet; }

/* ── Helpers ─────────────────────────────────────────────────────────────── */
static void sendPersonalityKeyboard() {
    String kb =
        "[[{\"text\":\"Funny\",\"callback_data\":\"/funny\"},{\"text\":\"Strict\",\"callback_data\":\"/strict\"}],"
        "[{\"text\":\"Formal\",\"callback_data\":\"/formal\"},{\"text\":\"Zen\",\"callback_data\":\"/zen\"}]]";
    bot.sendMessageWithInlineKeyboard(CHAT_ID, "Choose your study coach personality:", "", kb);
}

/* ── Message handler — library handleNewMessages() convention ────────────── */
static void handleNewMessages(int numNew) {
    for (int i = 0; i < numNew; i++) {
        telegramMessage &msg = bot.messages[i];
        String text = msg.text;
        String chat = msg.chat_id;

        // Acknowledge callback queries immediately to clear the loading spinner
        if (msg.type == F("callback_query")) {
            bot.answerCallbackQuery(msg.query_id, "");
        }

        if (text == "/start" || text == "/start@studycoachesp32sbot" ||
            text == "/personality" || text == "/personality@studycoachesp32sbot") {
            // Personality is locked once set — silently ignore repeat requests
            if (!sPersonalitySet) {
                sendPersonalityKeyboard();
            }

        } else if (text == "/funny") {
            sPersonality =
                "You are a funny, lovable study buddy who speaks in casual Singlish-English. "
                "Your tone is warm, teasing, and dramatic — like a close friend who roasts the user just enough to make them laugh but never feels mean. "
                "You use Singlish expressions naturally: 'lah', 'leh', 'sia', 'walao', 'confirm plus chop', 'die die must', 'aiyah', etc. "
                "You celebrate small wins dramatically and react to bad conditions like it is a personal tragedy. "
                "You are still genuinely helpful — always include exactly one clear, practical suggestion. "
                "Never be vulgar, hateful, or overly harsh. "
                "Keep the tone light, fun, and encouraging overall.";
            sPersonalitySet = true;
            bot.sendMessage(chat, "Funny Singlish mode activated lah! Let's go sia!", "");

        } else if (text == "/strict") {
            sPersonality =
                "You are a strict, no-nonsense study coach with high standards. "
                "You speak in short, direct sentences. You do not sugarcoat. "
                "You acknowledge what went well, but immediately pivot to what must improve. "
                "You treat the user as someone capable of doing better — your bluntness comes from high expectations, not cruelty. "
                "You do not use humor, emojis (except the allowed one), filler phrases, or small talk. "
                "You call out poor study conditions as unacceptable and demand corrective action. "
                "Always end with exactly one firm, actionable directive. "
                "Do not praise unless it is genuinely deserved.";
            sPersonalitySet = true;
            bot.sendMessage(chat, "Strict mode activated. No excuses. Get to work.", "");

        } else if (text == "/formal") {
            sPersonality    = PERSONALITY_FORMAL;
            sPersonalitySet = true;
            bot.sendMessage(chat, "Formal mode activated. Your session debrief will be delivered professionally.", "");

        } else if (text == "/zen") {
            sPersonality =
                "You are a calm, mindful wellness coach who guides the user with gentle wisdom. "
                "Your tone is soft, unhurried, and grounding — like a trusted mentor who believes in the user completely. "
                "You never rush or alarm. Even when the study environment was poor, you reframe it as an opportunity to grow and adjust. "
                "Use peaceful, flowing language. Avoid harsh words or negative framing. "
                "Celebrate the act of showing up and studying as meaningful in itself. "
                "Gently surface the one most important environmental insight, and offer it as a kind suggestion rather than a correction. "
                "End with exactly one calming, practical recommendation that nurtures both focus and wellbeing.";
            sPersonalitySet = true;
            bot.sendMessage(chat, "Zen mode activated. Breathe. You are doing well.", "");
        }
    }
}

/* ── Blocks until personality chosen or 20 s timeout → defaults to Formal ── */
static void waitForPersonality() {
    unsigned long deadline = millis() + 20000;

    while (!sPersonalitySet) {
        if (millis() >= deadline) {
            sPersonality    = PERSONALITY_FORMAL;
            sPersonalitySet = true;
            bot.sendMessage(CHAT_ID, "No selection received — defaulting to Formal mode.", "");
            Serial.println("[Telegram] Personality timeout — defaulted to Formal");
            return;
        }

        int numNew = bot.getUpdates(bot.last_message_received + 1);
        while (numNew) {
            handleNewMessages(numNew);
            numNew = bot.getUpdates(bot.last_message_received + 1);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ── Command registration ────────────────────────────────────────────────── */
static void bot_setup() {
    const String commands = F("["
        "{\"command\":\"start\",       \"description\":\"Welcome message and personality menu\"},"
        "{\"command\":\"personality\", \"description\":\"Choose your study coach tone\"}"
    "]");
    bot.setMyCommands(commands);
}

/* ── Task ────────────────────────────────────────────────────────────────── */
void vTelegramTask(void *pvParameters) {
    (void)pvParameters;

    char rxBuffer[GEMINI_RESPONSE_MAX_LEN];

    client.setInsecure();
    if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(10000)) == pdTRUE) {
        bot_setup();
        bot.sendMessage(CHAT_ID, "ESP32 online! Please choose your study coach personality:", "");
        sendPersonalityKeyboard();
        waitForPersonality();
        xSemaphoreGive(gNetworkMutex);
    }

    while (1) {
        unsigned long now = millis();

        // Poll Telegram on a timed interval — take mutex once, drain all updates, release
        if (now - sLastPollTime > BOT_POLL_MS) {
            sLastPollTime = now;
            if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
                int numNew = bot.getUpdates(bot.last_message_received + 1);
                while (numNew) {
                    handleNewMessages(numNew);
                    numNew = bot.getUpdates(bot.last_message_received + 1);
                }
                xSemaphoreGive(gNetworkMutex);
            }
        }

        // Send Gemini report if one is queued — short timeout so polling is not starved
        if (xQueueReceive(gTelegramQueue, &rxBuffer, pdMS_TO_TICKS(100)) == pdPASS) {
            Serial.println("[Telegram] Report dequeued");
            now = millis();

            if ((sLastTelegramSend != 0) && (now - sLastTelegramSend) < TELEGRAM_COOLDOWN_MS) {
                Serial.println("[Telegram] Cooldown active — dropping duplicate report.");
                continue;
            }

            if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(15000)) == pdTRUE) {
                if (bot.sendMessage(CHAT_ID, String(rxBuffer), "")) {
                    Serial.println("[Telegram] Report sent successfully");
                } else {
                    Serial.println("[Telegram] Report send failed");
                }
                sLastTelegramSend = millis();
                xSemaphoreGive(gNetworkMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
