#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "gemini.h"
#include "passwords.h"
#include "shared_data.h"

ESP32_AI_Connect aiClient("gemini", GEMINI_KEY, GEMINI_MODEL);

/* ── Cooldown ────────────────────────────────────────────────────────────── */
#define GEMINI_COOLDOWN_MS   60000   // 60 seconds minimum between API calls
static unsigned long sLastGeminiCall = 0;

/* ── WiFi ────────────────────────────────────────────────────────────────── */
void Gemini_Init(void) {
    aiClient.setChatMaxTokens(300);
    aiClient.setChatTemperature(0.7);
    aiClient.setChatSystemRole("You are a study environment AI assistant.");
}

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

    if (xSemaphoreTake(gNetworkMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
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
    } else {
        Serial.println("[Gemini] Response received successfully");
        Serial.print("[Gemini] "); Serial.println(response);
        if (xSemaphoreTake(gGeminiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            strncpy(gGeminiResponse, response.c_str(), GEMINI_RESPONSE_MAX_LEN - 1);
            gGeminiResponse[GEMINI_RESPONSE_MAX_LEN - 1] = '\0';
            xSemaphoreGive(gGeminiMutex);
        } else {
            Serial.println("[Gemini] Warning: could not acquire mutex to store response");
        }
    }
    
    return response;
}

void vGeminiTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        if (gGeminiTrigger) {
            gGeminiTrigger = false;   // clear before calling, not after

            // Build a prompt with current sensor context
            if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                float t = gSensorData.esp_temp;
                float h = gSensorData.esp_humidity;
                xSemaphoreGive(gSensorMutex);

                // String prompt = "The focus session just ended. Temp was " + String(t, 1)
                //               + "C, humidity " + String(h, 1)
                //               + "%. Give a short study session recap and tip.";
                String prompt = "is you awake?";
                postGemini(prompt);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));   // poll every 500ms
    }
}