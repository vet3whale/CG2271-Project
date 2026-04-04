#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "gemini.h"
#include "passwords.h"
#include "uart_packet.h"
#include "shared_data.h"

/* ── Cooldown ────────────────────────────────────────────────────────────── */
#define GEMINI_COOLDOWN_MS   20000   // 60 seconds minimum between API calls
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
    if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        aiClient.setChatMaxTokens(300);
        aiClient.setChatTemperature(0.7);
        aiClient.setChatSystemRole(
            "You are a funny study buddy. "
            "You reply in casual Singlish-English, playful and teasing, like a close friend roasting the user a bit. "
            "Be dramatic and funny, but still helpful. "
            "Do not be vulgar, hateful, or overly harsh. "
            "Always include one practical suggestion."
        );
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

        // Send the buffer to the queue (do not wait if full)
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
    const float IDEAL_TEMP_MIN = 24.0f;
    const float IDEAL_TEMP_MAX = 27.0f;
    const float IDEAL_HUM_MIN  = 40.0f;
    const float IDEAL_HUM_MAX  = 60.0f;
    const uint16_t IDEAL_LIGHT_MIN = 400;
    const uint16_t IDEAL_LIGHT_MAX = 700;
    const uint16_t IDEAL_SOUND_MAX = 120;

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
                temp = gSensorData.esp_temp;
                humidity = gSensorData.esp_humidity;
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
                "You are a study environment assistant. "
                "A study session has just ended.\n\n"

                "Measured session data:\n"
                "- Study duration: " + timeStr + "\n"
                "- Average temperature: " + String(temp, 1) + " C\n"
                "- Average humidity: " + String(humidity, 1) + " %\n"
                "- Average light: " + String(light) + "\n"
                "- Average sound: " + String(sound) + "\n"
                "- Sound triggered: " + String(soundTriggered) + "\n"
                "- Overall environment condition: " + envStr + "\n\n"

                "Ideal study environment:\n"
                "- Temperature: " + String(IDEAL_TEMP_MIN, 1) + " to " + String(IDEAL_TEMP_MAX, 1) + " C\n"
                "- Humidity: " + String(IDEAL_HUM_MIN, 1) + " to " + String(IDEAL_HUM_MAX, 1) + " %\n"
                "- Light: " + String(IDEAL_LIGHT_MIN) + " to " + String(IDEAL_LIGHT_MAX) + "\n"
                "- Sound: below " + String(IDEAL_SOUND_MAX) + "\n"
                "- Repeated sound triggers are bad for focus\n\n"

                "Instructions:\n"
                "1. Praise the user briefly for completing the session.\n"
                "2. Compare the measured values against the ideal ranges.\n"
                "3. Mention only the most important 1 or 2 environment issues.\n"
                "4. Give exactly one practical suggestion.\n"
                "5. If conditions were generally good, say so clearly.\n"
                "6. Keep the response under 350 characters.\n"
                "7. Reply like a funny friend in casual Singlish. "
                    "If the environment is bad, roast the user lightly, for example like "
                    "'brooo u tweaking sia, why you study in the dark'. "
                    "Be playful, dramatic, and short. "
                    "Mention the biggest problem only, then give one practical fix. "
                    "Keep it under 220 characters. "
                    "Use exactly one emoji. "
                    "No markdown, no asterisks.";

            postGemini(prompt);
        }
    }
}