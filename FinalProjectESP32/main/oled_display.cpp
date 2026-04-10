// oled_display.cpp
#include "oled_display.h"
#include "shared_data.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart_packet.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static unsigned long sSessionStart = 0;
static unsigned long sPausedAt = 0;
static unsigned long sTotalPaused = 0;
static bool sWasRunning = false;
static unsigned long sFinalElapsed = 0;

// session summary message
static char sGeminiLines[60][22];
static int sGeminiLineCount = 0;
static int sScrollPixelY = 0;
static int sScrollHoldTicks = 0;
static bool sScrollDone = false;
static bool sShowingGemini = false;

static uint8_t sEnvCondition = ENV_GOOD;  // default

static String formatTime(unsigned long seconds) {
  unsigned long h = seconds / 3600;
  unsigned long m = (seconds % 3600) / 60;
  unsigned long s = seconds % 60;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
  return String(buf);
}

// emoji to describe environment level
static void drawFace(int x, int y, uint8_t envCondition) {
  int cx = x + 10;
  int cy = y + 10;
  int r = 10;

  // face
  display.drawCircle(cx, cy, r, SSD1306_WHITE);

  // eyes
  display.fillCircle(cx - 3, cy - 3, 1, SSD1306_WHITE);
  display.fillCircle(cx + 3, cy - 3, 1, SSD1306_WHITE);

  // mouth
  if (envCondition == ENV_GOOD) {
    for (int dx = -4; dx <= 4; dx++) {
      int dy = -(dx * dx) / 6 + 6;
      display.drawPixel(cx + dx, cy + dy, SSD1306_WHITE);  // smile
    }
  } else if (envCondition == ENV_MODERATE) {
    display.drawLine(cx - 4, cy + 4, cx + 4, cy + 4, SSD1306_WHITE);  // straight face
  } else {
    for (int dx = -4; dx <= 4; dx++) {
      int dy = (dx * dx) / 6 + 3;
      display.drawPixel(cx + dx, cy + dy, SSD1306_WHITE);  // frown
    }
  }
}

static void stripNonAscii(char* dst, const char* src, size_t maxLen) {
  size_t j = 0;
  bool lastWasSpace = false;
  for (size_t i = 0; src[i] != '\0' && j < maxLen - 1; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c >= 0x20 && c <= 0x7E) {  // printable ASCII only - only letters
      bool isSpace = (c == ' ');
      if (isSpace && lastWasSpace) continue;  // collapse double spaces
      dst[j++] = (char)c;
      lastWasSpace = isSpace;
    }
  }
  // trim trailing space
  while (j > 0 && dst[j - 1] == ' ') j--;
  dst[j] = '\0';
}

static void wrapGeminiText(const char* msg) {
  char clean[GEMINI_RESPONSE_MAX_LEN];
  stripNonAscii(clean, msg, GEMINI_RESPONSE_MAX_LEN);

  sGeminiLineCount = 0;
  sScrollPixelY = 0;
  sScrollHoldTicks = 0;
  sScrollDone = false;

  const int WRAP = 21;  // chars per line
  int len = strlen(clean);
  int pos = 0;

  while (pos < len && sGeminiLineCount < 60) {
    int remaining = len - pos;
    if (remaining <= WRAP) {
      strncpy(sGeminiLines[sGeminiLineCount], clean + pos, remaining);
      sGeminiLines[sGeminiLineCount][remaining] = '\0';
      sGeminiLineCount++;
      break;
    }
    // break on last space within the WRAP window
    int breakAt = pos + WRAP;
    for (int i = pos + WRAP; i > pos; i--) {
      if (clean[i] == ' ') {
        breakAt = i;
        break;
      }
    }
    int lineLen = breakAt - pos;
    strncpy(sGeminiLines[sGeminiLineCount], clean + pos, lineLen);
    sGeminiLines[sGeminiLineCount][lineLen] = '\0';
    sGeminiLineCount++;
    pos = breakAt + (clean[breakAt] == ' ' ? 1 : 0);
  }
}

static bool showGeminiScroll() {
  const int HEADER_H = 11; // size-1 header: 8px text + 3px gap
  const int TEXT_AREA_H = SCREEN_HEIGHT - HEADER_H; // 53px
  const int CHAR_H = 8; // size-2 font height
  const int LINE_GAP = 5; // extra gap between lines
  const int LINE_H = CHAR_H + LINE_GAP; // 20px per line
  const int SCROLL_SPEED = 6;

  display.clearDisplay();

  // fixed header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Study Coach:");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  display.setTextSize(1);
  for (int i = 0; i < sGeminiLineCount; i++) {
    int y = HEADER_H + (i * LINE_H) - sScrollPixelY;
    if (y + CHAR_H <= HEADER_H) continue; // above viewport
    if (y >= SCREEN_HEIGHT) break; // below viewport
    display.setCursor(0, y);
    display.print(sGeminiLines[i]);
  }

  display.display();

  // scroll until last line and then hold 3s
  int totalTextH = sGeminiLineCount * LINE_H;
  int maxScroll = max(0, totalTextH - TEXT_AREA_H);

  if (sScrollPixelY < maxScroll) {
    sScrollPixelY = min(sScrollPixelY + SCROLL_SPEED, maxScroll);
  } else {
    sScrollHoldTicks++;
    if (sScrollHoldTicks >= 6) {  // 6 × 500ms = 3s hold
      sScrollDone = true;
    }
  }

  return sScrollDone;
}

static void showLoading() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(25, 20);
  display.println("Study Coach");

  display.setCursor(30, 36);
  display.println("Loading...");

  display.display();
}

// display states
static void showRunning(unsigned long elapsed, float temp, float hum) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 0);
  display.println("STUDY SESSION");
  display.drawLine(0, 10, 100, 10, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 18);
  display.println(formatTime(elapsed));
  display.setTextSize(1);

  // live temp & humidity
  display.setCursor(0, 42);
  display.printf("Temp: %.1f C", temp);
  display.setCursor(0, 52);
  display.printf("Hum:  %.1f %%", hum);

  display.setCursor(85, 52);
  display.println("RUN");

  // emoji face top right
  drawFace(105, 0, sEnvCondition);

  display.display();
}

static void showPaused(unsigned long elapsed, float temp, float hum) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 0);
  display.println("STUDY SESSION");
  display.drawLine(0, 10, 100, 10, SSD1306_WHITE);
  display.fillRect(44, 14, 10, 20, SSD1306_WHITE);
  display.fillRect(60, 14, 10, 20, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(28, 38);
  display.print("PAUSED  ");
  display.print(formatTime(elapsed));

  // live avg temp & humidity while paused
  display.setCursor(0, 50);
  display.printf("T:%.1fC  H:%.1f%%", temp, hum);

  // emoji face top right
  drawFace(105, 0, sEnvCondition);

  display.display();
}

static void showSummary(float temp, float hum) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(25, 0);
  display.println("SESSION SUMMARY");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 15);
  display.print("Time: ");
  display.println(formatTime(sFinalElapsed));

  display.setCursor(0, 30);
  display.printf("Temp: %.1f C", temp);

  display.setCursor(0, 42);
  display.printf("Hum:  %.1f %%", hum);

  display.display();
}

static void showIdle() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 10);
  display.println("No active session");
  display.setCursor(22, 30);
  display.println("Tap to start!");
  display.drawRect(0, 0, 127, 63, SSD1306_WHITE);
  display.display();
}


// Init
void OLED_Init() {
  Wire.begin(6, 7);
  bool started = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  showLoading();
}

void vOLEDTask(void* pvParameters) {
  (void)pvParameters;

  uint8_t onOff = 0;
  uint8_t paused = 0;
  float temp = 0.0f;
  float hum = 0.0f;

  uint8_t summaryTicksLeft = 0;

  Serial.println("[OLED] Task started");

  while (1) {
    if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      onOff = gSensorData.on_off;
      paused = gSensorData.paused;
      temp = gSensorData.esp_temp;
      hum = gSensorData.esp_humidity;
      sEnvCondition = gSensorData.env_condition;  // read env condition
      xSemaphoreGive(gSensorMutex);
    }
    if (!gSystemReady || temp <= 0.0f || hum <= 0.0f) {
      showLoading();
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    unsigned long now = millis();

    if (onOff == 1 && paused == 0) {
      // runninggg
      if (!sWasRunning) {
        if (sSessionStart == 0) {
          // new session
          sSessionStart = now;
        } else if (sPausedAt != 0) {
          // resuming from a pause, add to total paused time
          sTotalPaused += (now - sPausedAt);
          sPausedAt = 0;
        }
        sWasRunning = true;
      }
      unsigned long elapsed = (now - sSessionStart - sTotalPaused) / 1000;
      showRunning(elapsed, temp, hum);

    } else if (onOff == 1 && paused == 1) {
      // paused
      if (sWasRunning) {
        sPausedAt = now; sWasRunning = false;
      }

      unsigned long elapsed = 0;
      if (sSessionStart != 0) {
        elapsed = (sPausedAt - sSessionStart - sTotalPaused) / 1000;
      }
      showPaused(elapsed, temp, hum);

    } else {
      // off
      if (sWasRunning || sSessionStart != 0) {
        // Save the final time before clearing the session variables
        if (sWasRunning) {
            sFinalElapsed = (now - sSessionStart - sTotalPaused) / 1000;
        } else {
            sFinalElapsed = (sPausedAt - sSessionStart - sTotalPaused) / 1000;
        }
        
        sWasRunning = false;
        sSessionStart = 0; 
        sTotalPaused = 0;
        sPausedAt = 0;
        sShowingGemini = false;  // reset any leftover scroll state
        summaryTicksLeft = 30; 
      }
      if (summaryTicksLeft > 0) {
        // show session summary
        showSummary(temp, hum);
        summaryTicksLeft--;

      } else if (sShowingGemini) {
        // scroll Gemini response
        if (showGeminiScroll()) {
          sShowingGemini = false;
          // mark message as consumed
          if (xSemaphoreTake(gGeminiMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            gGeminiOLEDReady = false;
            xSemaphoreGive(gGeminiMutex);
          }
        }

      } else {
        // check for a pending Gemini message, otherwise idle
        bool ready = false;
        char msgBuf[GEMINI_RESPONSE_MAX_LEN];
        if (xSemaphoreTake(gGeminiMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          ready = gGeminiOLEDReady;
          if (ready) strncpy(msgBuf, gGeminiOLEDMsg, GEMINI_RESPONSE_MAX_LEN);
          xSemaphoreGive(gGeminiMutex);
        }

        if (ready) {
          wrapGeminiText(msgBuf);
          sShowingGemini = true;
        } else {
          showIdle();
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}