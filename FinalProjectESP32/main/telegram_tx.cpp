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

#define TELEGRAM_COOLDOWN_MS      15000
#define CMD_POLL_FAST_MS           3000   // poll interval before a tone is chosen
#define CMD_REOPEN_CHECK_MS       30000   // how often to check for /personality after tone is set
static unsigned long sLastTelegramSend  = 0;
static unsigned long sLastReopenCheck   = 0;

/* ── Personality ─────────────────────────────────────────────────────────── */
static volatile bool sPersonalitySet = false;   // false until user picks a tone
static String sPersonality =
    // FUNNY (default)
    "You are a funny, lovable study buddy who speaks in casual Singlish-English. "
    "Your tone is warm, teasing, and dramatic — like a close friend who roasts the user just enough to make them laugh but never feels mean. "
    "You use Singlish expressions naturally: 'lah', 'leh', 'sia', 'walao', 'confirm plus chop', 'die die must', 'aiyah', etc. "
    "You celebrate small wins dramatically and react to bad conditions like it is a personal tragedy. "
    "You are still genuinely helpful — always include exactly one clear, practical suggestion. "
    "Never be vulgar, hateful, or overly harsh. "
    "Keep the tone light, fun, and encouraging overall.";

String Telegram_GetPersonality() {
    return sPersonality;
}

static void checkForCommands() {
    int numNew = bot.getUpdates(bot.last_message_received + 1);
    for (int i = 0; i < numNew; i++) {
        // Inline keyboard buttons arrive as callback queries — use callback_query_data.
        // Regular typed commands arrive as plain text messages — use text.
        String text = bot.messages[i].callback_query_data.length() > 0
                      ? bot.messages[i].callback_query_data
                      : bot.messages[i].text;
        String chat = bot.messages[i].chat_id;

        if (text == "/start" || text == "/personality") {
            sPersonalitySet = false;   // re-open selection — resume fast polling
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
                "You are a funny, lovable study buddy who speaks in casual Singlish-English. "
                "Your tone is warm, teasing, and dramatic — like a close friend who roasts the user just enough to make them laugh but never feels mean. "
                "You use Singlish expressions naturally: 'lah', 'leh', 'sia', 'walao', 'confirm plus chop', 'die die must', 'aiyah', etc. "
                "You celebrate small wins dramatically and react to bad conditions like it is a personal tragedy. "
                "You are still genuinely helpful — always include exactly one clear, practical suggestion. "
                "Never be vulgar, hateful, or overly harsh. "
                "Keep the tone light, fun, and encouraging overall.";
            sPersonalitySet = true;
            bot.sendMessage(chat, "😂 Wah, funny Singlish mode activated lah! Let's go sia!", "");

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
            bot.sendMessage(chat, "😤 Strict mode activated. No excuses. Get to work.", "");

        } else if (text == "/formal") {
            sPersonality =
                "You are a professional academic advisor providing a structured post-session debrief. "
                "Your language is formal, composed, and precise — no slang, contractions, or casual expressions. "
                "You present the study environment data in a measured, analytical tone, similar to a written academic report. "
                "Acknowledge the user's effort professionally, then provide a clear, evidence-based assessment of the study conditions. "
                "Highlight the most significant environmental factor affecting study quality. "
                "Conclude with exactly one well-reasoned, actionable recommendation phrased as professional advice. "
                "Maintain a respectful, objective tone throughout.";
            sPersonalitySet = true;
            bot.sendMessage(chat, "💼 Formal mode activated. Your session debrief will be delivered professionally.", "");

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
            bot.sendMessage(chat, "🧘 Zen mode activated. Breathe. You are doing well.", "");
        }
    }
}

/* ── Task — unchanged except checkForCommands() added at top of loop ─────── */
void vTelegramTask(void *pvParameters) {
    char rxBuffer[GEMINI_RESPONSE_MAX_LEN];

    while (1) {
        unsigned long now_ms = millis();

        if (!sPersonalitySet) {
            // No tone chosen yet — poll frequently so the button press is caught quickly
            checkForCommands();
            xQueueReceive(gTelegramQueue, &rxBuffer, pdMS_TO_TICKS(CMD_POLL_FAST_MS));
            continue;
        }

        // Tone is set — only check every 30 s in case user sends /personality to change it
        if ((now_ms - sLastReopenCheck) >= CMD_REOPEN_CHECK_MS) {
            sLastReopenCheck = now_ms;
            checkForCommands();
            // If /personality was sent, sPersonalitySet is now false — loop will fast-poll next iteration
            if (!sPersonalitySet) continue;
        }

        if (xQueueReceive(gTelegramQueue, &rxBuffer, pdMS_TO_TICKS(CMD_REOPEN_CHECK_MS)) == pdPASS) {
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

                // Notify user that the report is on its way
                bot.sendMessage(CHAT_ID, "⏳ Generating your session report...", "");

                if (!(sLastTelegramSend && (now - sLastTelegramSend) < TELEGRAM_COOLDOWN_MS) && bot.sendMessage(CHAT_ID, String(rxBuffer), "")) {
                    Serial.println("[Telegram] Send success");
                    bot.sendMessage(CHAT_ID, "✅ Report delivered!", "");
                } else {
                    Serial.println("[Telegram] Send failed");
                    bot.sendMessage(CHAT_ID, "❌ Failed to deliver report. Please check the device.", "");
                }
                sLastTelegramSend = millis();
                xSemaphoreGive(gNetworkMutex);
            }

            // Short rest to let the WiFi hardware stabilize
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}