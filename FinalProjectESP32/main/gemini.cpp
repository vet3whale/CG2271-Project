#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "gemini.h"
#include "passwords.h"
#include "uart_packet.h"
#include "shared_data.h"
#include "telegram_tx.h"   /* Telegram_GetPersonality() */

/* ── Cooldown ────────────────────────────────────────────────────────────── */
#define GEMINI_COOLDOWN_MS   20000   // 20 seconds minimum between API calls
static unsigned long sLastGeminiCall = 0;

/* ── Gemini ──────────────────────────────────────────────────────────────── */
String postGemini(const String &prompt) {
    unsigned long now = millis();

    if (sLastGeminiCall != 0 && (now - sLastGeminiCall) < GEMINI_COOLDOWN_MS) {
        unsigned long remaining = (GEMINI_COOLDOWN_MS - (now - sLastGeminiCall)) / 1000;
        Serial.print("[Gemini] Cooldown active");
        return "";
    }

    Serial.println("[Gemini] Sending request...");
    sLastGeminiCall = now;

    String response = "";

    ESP32_AI_Connect aiClient("gemini", GEMINI_KEY, GEMINI_MODEL);
    if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(15000)) == pdTRUE) {
        aiClient.setChatMaxTokens(300);
        aiClient.setChatTemperature(0.7);
        aiClient.setChatSystemRole(Telegram_GetPersonality().c_str());  /* ← changed */
        response = aiClient.chat(prompt);
        xSemaphoreGive(gNetworkMutex);
    } else {
        Serial.println("[Gemini] Failed to acquire network mutex");
        return "";
    }

    if (response.isEmpty()) {
        String error = aiClient.getLastError();
        Serial.print("[Gemini] Error: ");
        Serial.println(error);

        if (error.indexOf("403") >= 0) {
            Serial.println("[Gemini] 403 = API key rejected — check aistudio.google.com/apikey");
        } else if (error.indexOf("429") >= 0) {
            Serial.println("[Gemini] 429 = Rate limit hit — cooldown will prevent this next time");
        } else if (error.indexOf("401") >= 0) {
            Serial.println("[Gemini] 401 = Unauthorised — wrong key format");
        }
    }
    if (response.length() > 0) {
        char msgBuffer[GEMINI_RESPONSE_MAX_LEN];
        strncpy(msgBuffer, response.c_str(), GEMINI_RESPONSE_MAX_LEN - 1);
        msgBuffer[GEMINI_RESPONSE_MAX_LEN - 1] = '\0';

        if (xQueueSend(gTelegramQueue, &msgBuffer, 0) != pdPASS) {
            Serial.println("[Gemini] Queue full, message dropped");
        } else {
            Serial.println("[Gemini] Message queued for Telegram");
        }
    }

    return response;
}

static const char* geminiEnvConditionStr(uint8_t cond)
{
    switch (cond) {
        case ENV_GOOD:     return "GOOD";
        case ENV_TOO_DARK: return "TOO DARK";
        case ENV_TOO_LOUD: return "TOO LOUD";
        case ENV_TOO_HOT:  return "TOO HOT";
        case ENV_TOO_COLD: return "TOO COLD";
        case ENV_MODERATE: return "MODERATE";
        case ENV_POOR:     return "POOR";
        default:           return "UNKNOWN";
    }
}

void vGeminiTask(void *pvParameters) {
    (void)pvParameters;

    // Ideal targets
    const float IDEAL_TEMP_MIN = 22.0f;
    const float IDEAL_TEMP_MAX = 31.0f;
    const float IDEAL_HUM_MIN  = 40.0f;
    const float IDEAL_HUM_MAX  = 75.0f;
    const uint16_t IDEAL_LIGHT_MIN = 0;
    const uint16_t IDEAL_LIGHT_MAX = 700;
    const uint16_t IDEAL_SOUND_MAX = 1800;

    while (1) {
        if (xSemaphoreTake(gGeminiSemaphore, portMAX_DELAY) == pdTRUE) {

            float temp = 0.0f;
            float humidity = 0.0f;
            uint16_t light = 0;
            uint16_t sound = 0;
            uint8_t soundTriggered = 0;
            uint8_t envCondition = 0;
            uint32_t duration_sec = 0;

            if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                temp = gSensorData.temp;
                humidity = gSensorData.hum;
                light = gSensorData.light_raw;
                sound = gSensorData.sound_raw;
                soundTriggered = gSensorData.sound_triggered;
                envCondition = gSensorData.env_condition;
                duration_sec = gSensorData.session_duration_sec;
                xSemaphoreGive(gSensorMutex);
            }

            uint32_t mins = duration_sec / 60;
            uint32_t secs = duration_sec % 60;
            String timeStr = String(mins) + " minutes and " + String(secs) + " seconds";

            String envStr = String(geminiEnvConditionStr(envCondition));

            String prompt =
                "A study session has just ended.\n\n"

                "Measured session data:\n"
                "- Study duration: " + timeStr + "\n"
                "- Average temperature: " + String(temp, 1) + " C\n"
                "- Average humidity: " + String(humidity, 1) + " %\n"
                "- Average light: " + String(light) + "\n"
                "- Average sound: " + String(sound) + "\n"
                "- Sound trigger count in 30s: " + String(soundTriggered) + "\n"
                "- Overall environment condition: " + envStr + "\n\n"

                "Ideal study environment:\n"
                "- Temperature: " + String(IDEAL_TEMP_MIN, 1) + " to " + String(IDEAL_TEMP_MAX, 1) + " C\n"
                "- Humidity: " + String(IDEAL_HUM_MIN, 1) + " to " + String(IDEAL_HUM_MAX, 1) + " %\n"
                "- Light: " + " below " + String(IDEAL_LIGHT_MAX) + "\n"
                "- Sound: below " + String(IDEAL_SOUND_MAX) + "\n"
                "- Repeated sound triggers are bad for focus\n\n"

                "Priority of issues:\n"
                "1. Light is the highest priority. Higher value in Light means dimmer.\n"
                "2. Sound is the second highest priority.\n"
                "3. Temperature should be mentioned only if it is far outside the ideal range.\n"
                "4. Humidity can be mentioned only if it is clearly uncomfortable.\n\n"
                "5. Never judge, comment on, or reference the study duration — do not call it short, long, good, or bad.\n"

                "Instructions:\n"
                "1. Start with exactly: Study Session Completed! Time: " + timeStr + "\n"
                "2. On the next line, write exactly: Average Temperature and Humidity: "
                    + String(temp, 1) + " C, " + String(humidity, 1) + " %\n"
                "3. On the next line, write exactly: Suggestions: \n"
                "4. In Suggestions, praise the user positively for completing the session — never mention or imply anything about the duration.\n"
                "5. Compare measured values against the ideal ranges silently, but mention only the biggest 1 issue, or 2 issues only if both are important.\n"
                "6. Always prioritize bad lighting first, then noisy environment.\n"
                "7. If light is bad, talk about light instead of sound unless sound is much worse.\n"
                "8. Ignore temperature unless it is very far from ideal.\n"
                "9. Give exactly one practical suggestion.\n"
                "10. If conditions were generally good, say so clearly.\n"
                "11. Deliver the Suggestions text in the tone and personality described in your system role — do not default to any particular style.\n"
                "12. Use exactly one emoji total.\n"
                "13. No markdown, no bullet points, no asterisks.\n"
                "14. Keep the whole reply concise.";

            postGemini(prompt);
        }
    }
}