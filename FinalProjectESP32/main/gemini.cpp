#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "gemini.h"
#include "passwords.h"
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
        aiClient.setChatSystemRole("You are a study environment AI assistant.");
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

void vGeminiTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        if (xSemaphoreTake(gGeminiSemaphore, portMAX_DELAY) == pdTRUE) {
            
            float t = 0.0;
            float h = 0.0;
            uint32_t duration_sec = 0;
            if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                t = gSensorData.esp_temp;
                h = gSensorData.esp_humidity;
                duration_sec = gSensorData.session_duration_sec;
                xSemaphoreGive(gSensorMutex);
            }

            uint32_t mins = duration_sec / 60;
            uint32_t secs = duration_sec % 60;
            String timeStr = String(mins) + " minutes and " + String(secs) + " seconds";

            // Build the prompt with the newly calculated time
            String prompt = "The focus session just ended. The user studied for " + timeStr + 
                            ". Temp was " + String(t, 1) + "C, humidity " + String(h, 1) + 
                            "%. Give a short study session recap praising them for their time " +
                            "and one tip based on the environment. Keep response to 400 characters. " +
                            "Include an emoji and no * in the text.";
            
            postGemini(prompt);
        }
    }
}