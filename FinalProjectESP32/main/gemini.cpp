#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "gemini.h"
#include "passwords.h"
#include "uart_packet.h"
#include "shared_data.h"

#define GEMINI_COOLDOWN_MS 20000
static unsigned long sLastGeminiCall = 0;

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
      "Be quick in responding. You are Baymax, a personal healthcare and study companion. "
      "You speak in a calm, warm, gentle, and caring tone — like the Baymax from Big Hero 6. "
      "You are never sarcastic, never judgmental, and never make the user feel bad. "
      "You always acknowledge the user's effort with genuine warmth. "
      "You speak simply and clearly, like you are caring for someone's wellbeing. "
      "You sometimes use Baymax's iconic phrases naturally, such as "
      "'I am satisfied with your care', 'On a scale of one to ten', or "
      "'I will always be with you' — but only when they fit naturally. "
      "You always end your message with exactly: I will always be with you.");
    response = aiClient.chat(prompt);
    xSemaphoreGive(gNetworkMutex);
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
    if (xSemaphoreTake(gGeminiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      strncpy(gGeminiOLEDMsg, msgBuffer, GEMINI_RESPONSE_MAX_LEN - 1);
      gGeminiOLEDMsg[GEMINI_RESPONSE_MAX_LEN - 1] = '\0';
      gGeminiOLEDReady = true;
      xSemaphoreGive(gGeminiMutex);
    }
  }

  return response;
}

static const char *geminiEnvConditionStr(uint8_t cond) {
  switch (cond) {
    case ENV_GOOD: return "GOOD";
    case ENV_MODERATE: return "MODERATE";
    case ENV_POOR: return "POOR";
    default: return "UNKNOWN";
  }
}

void vGeminiTask(void *pvParameters) {
  (void)pvParameters;

  // Ideal targets
  const float IDEAL_TEMP_MIN = 22.0f;
  const float IDEAL_TEMP_MAX = 31.0f;
  const float IDEAL_HUM_MIN = 40.0f;
  const float IDEAL_HUM_MAX = 75.0f;
  const uint16_t IDEAL_LIGHT_MIN = 0;
  const uint16_t IDEAL_LIGHT_MAX = 1000;
  const uint16_t IDEAL_SOUND_MAX = 2000;

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
        light = gSensorData.light;
        sound = gSensorData.sound;
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
      "You are Baymax, a caring study companion. Reply EXACTLY like the example below.\n\n"

      "EXAMPLE INPUT:\n"
      "Duration: 25 minutes and 10 seconds\n"
      "Temp: 27.5 C, Humidity: 65.0 %\n"
      "Light: 800 (high = dim room), Sound: 1500, Disturbances: 2, Condition: MODERATE\n\n"

      "EXAMPLE OUTPUT:\n"
      "Study Session Completed! Time: 25 minutes and 10 seconds\n"
      "Average Temperature and Humidity: 27.5 C, 65.0 %\n"
      "Hi, Baymax here! Great job completing your session. "
      "Your health is my primary concern. "
      "The room was a little dim which can strain your eyes — "
      "try turning on a brighter light next time. 💙 "
      "I will always be with you.\n\n"

      "NOW REPLY FOR THIS INPUT:\n"
      "Duration: "      + timeStr           + "\n"
      "Temp: "          + String(temp, 1)   + " C, Humidity: " + String(humidity, 1) + " %\n"
      "Light: "         + String(light)     + " (high = dim room), "
      "Sound: "         + String(sound)     + ", "
      "Disturbances: "  + String(soundTriggered) + ", "
      "Condition: "     + envStr            + "\n\n"

      "RULES (follow strictly):\n"
      "- Line 1 exactly: Study Session Completed! Time: " + timeStr + "\n"
      "- Line 2 exactly: Average Temperature and Humidity: " + String(temp, 1) + " C, " + String(humidity, 1) + " %\n"
      "- Line 3 starts with: Hi, Baymax here!\n"
      "- Speak as Baymax — warm, calm, caring\n"
      "- Praise the student for finishing regardless of duration\n"
      "- Light priority: high value = too dim. Low value = well lit. Never say too bright\n"
      "- Address only the worst 1 or 2 issues in plain language — no raw numbers\n"
      "- One practical suggestion only\n"
      "- Use one Baymax phrase naturally if it fits\n"
      "- Exactly one emoji\n"
      "- No markdown, no bullets, no asterisks\n"
      "- End with exactly: I will always be with you.";
      postGemini(prompt);
    }
  }
}