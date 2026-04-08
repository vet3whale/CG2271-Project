// oled_display.cpp
#include "oled_display.h"
#include "shared_data.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart_packet.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS  0x3C

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ── Session timer ───────────────────────────────────────────────────────── */
static unsigned long sSessionStart = 0;
static unsigned long sPausedAt     = 0;
static unsigned long sTotalPaused  = 0;
static bool          sWasRunning   = false;

/* ── Session stats accumulators ──────────────────────────────────────────── */
static float    sTempSum      = 0.0f;
static float    sHumSum       = 0.0f;
static uint32_t sSampleCount  = 0;
static float    sAvgTemp      = 0.0f;
static float    sAvgHum       = 0.0f;
static unsigned long sFinalElapsed = 0;

/* ── Environment condition ───────────────────────────────────────────────── */
static uint8_t sEnvCondition = ENV_GOOD;  // default

static String formatTime(unsigned long seconds) {
    unsigned long h = seconds / 3600;
    unsigned long m = (seconds % 3600) / 60;
    unsigned long s = seconds % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
    return String(buf);
}

/* ── Emoji face drawn at top-right corner ────────────────────────────────── */
static void drawFace(int x, int y, uint8_t envCondition) {
    int cx = x + 10;
    int cy = y + 10;
    int r  = 10;

    // Face outline
    display.drawCircle(cx, cy, r, SSD1306_WHITE);

    // Eyes
    display.fillCircle(cx - 3, cy - 3, 1, SSD1306_WHITE);
    display.fillCircle(cx + 3, cy - 3, 1, SSD1306_WHITE);

    // Mouth
if (envCondition == ENV_GOOD) {
    // Smile — curve downward
    for (int dx = -4; dx <= 4; dx++) {
        int dy = -(dx * dx) / 6 + 6;
        display.drawPixel(cx + dx, cy + dy, SSD1306_WHITE);
    }
} else if (envCondition == ENV_MODERATE) {
    // Straight face
    display.drawLine(cx - 4, cy + 4, cx + 4, cy + 4, SSD1306_WHITE);
} else {
    // Frown — curve upward
    for (int dx = -4; dx <= 4; dx++) {
        int dy = (dx * dx) / 6 + 3;
        display.drawPixel(cx + dx, cy + dy, SSD1306_WHITE);
    }
}
}

/* ── Display states ──────────────────────────────────────────────────────── */
static void showRunning(unsigned long elapsed) {
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

    // Live temp & humidity
    display.setCursor(0, 42);
    display.printf("Temp: %.1f C", sAvgTemp);
    display.setCursor(0, 52);
    display.printf("Hum:  %.1f %%", sAvgHum);

    display.setCursor(85, 52);
    display.println("RUN");

    // Emoji face top right
    drawFace(105, 0, sEnvCondition);

    display.display();
}

static void showPaused(unsigned long elapsed) {
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

    // Live avg temp & humidity while paused
    display.setCursor(0, 50);
    display.printf("T:%.1fC  H:%.1f%%", sAvgTemp, sAvgHum);

    // Emoji face top right
    drawFace(105, 0, sEnvCondition);

    display.display();
}

static void showSummary() {
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
    display.printf("Avg Temp: %.1f C", sAvgTemp);

    display.setCursor(0, 42);
    display.printf("Avg Hum:  %.1f %%", sAvgHum);

    display.setCursor(0, 54);
    display.printf("Samples:  %lu", sSampleCount);

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

/* ── I2C Scanner ─────────────────────────────────────────────────────────── */
static void scanI2C() {
    Serial.println("[I2C] Scanning for devices...");
    bool found = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] Device found at 0x%02X\n", addr);
            found = true;
        }
    }
    if (!found) Serial.println("[I2C] No devices found — check SDA/SCL wiring and power");
    Serial.println("[I2C] Scan complete");
}

/* ── Init ────────────────────────────────────────────────────────────────── */
void OLED_Init() {
    Wire.begin(6, 7);
    Serial.println("[OLED] I2C initialised on SDA=GPIO6, SCL=GPIO7");
    scanI2C();

    bool started = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (!started) {
        Serial.println("[OLED] 0x3C failed — trying 0x3D...");
        started = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
        if (!started) {
            Serial.println("[OLED] Failed on both 0x3C and 0x3D — halting OLED task");
            vTaskDelete(NULL);
            return;
        } else {
            Serial.println("[OLED] Initialised at 0x3D");
        }
    } else {
        Serial.println("[OLED] Initialised at 0x3C");
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(25, 28);
    display.println("Study Coach");
    display.display();
    Serial.println("[OLED] Splash screen displayed");
    delay(1500);
}

/* ── Task ────────────────────────────────────────────────────────────────── */
void vOLEDTask(void *pvParameters) {
    (void)pvParameters;

    uint8_t onOff  = 0;
    uint8_t paused = 0;
    float   temp   = 0.0f;
    float   hum    = 0.0f;

    uint8_t summaryTicksLeft = 0;

    Serial.println("[OLED] Task started");

    while (1) {
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            onOff         = gSensorData.on_off;
            paused        = gSensorData.paused;
            temp          = gSensorData.esp_temp;
            hum           = gSensorData.esp_humidity;
            sEnvCondition = gSensorData.env_condition;  // read env condition
            xSemaphoreGive(gSensorMutex);
        } else {
            Serial.println("[OLED] Warning: could not take sensor mutex");
        }

        unsigned long now = millis();

        if (onOff == 1 && paused == 0) {
            // ── RUNNING ──
            summaryTicksLeft = 0;

            if (!sWasRunning) {
                if (sSessionStart == 0) {
                    sSessionStart = now;
                    sTotalPaused  = 0;
                    sTempSum      = 0.0f;
                    sHumSum       = 0.0f;
                    sSampleCount  = 0;
                    Serial.println("[OLED] Session started");
                } else {
                    sTotalPaused += (now - sPausedAt);
                    Serial.println("[OLED] Session resumed");
                }
                sWasRunning = true;
            }

            if (temp > 0.0f && hum > 0.0f) {
                sTempSum += temp;
                sHumSum  += hum;
                sSampleCount++;
            }

            if (sSampleCount > 0) {
                sAvgTemp = sTempSum / sSampleCount;
                sAvgHum  = sHumSum  / sSampleCount;
            }

            unsigned long elapsed = (now - sSessionStart - sTotalPaused) / 1000;
            showRunning(elapsed);

        } else if (onOff == 1 && paused == 1) {
            // ── PAUSED ──
            summaryTicksLeft = 0;

            if (sWasRunning) {
                sPausedAt   = now;
                sWasRunning = false;
                Serial.println("[OLED] Session paused");
            }

            unsigned long elapsed = (sPausedAt - sSessionStart - sTotalPaused) / 1000;
            showPaused(elapsed);

        } else {
            // ── OFF / IDLE ──
            if (sWasRunning || sSessionStart != 0) {
                sFinalElapsed = (now - sSessionStart - sTotalPaused) / 1000;
                if (sSampleCount > 0) {
                    sAvgTemp = sTempSum / sSampleCount;
                    sAvgHum  = sHumSum  / sSampleCount;
                }
                Serial.printf("[OLED] Session ended — elapsed=%lus avgT=%.1f avgH=%.1f samples=%lu\n",
                              sFinalElapsed, sAvgTemp, sAvgHum, sSampleCount);

                sWasRunning   = false;
                sSessionStart = 0;
                sTotalPaused  = 0;

                summaryTicksLeft = 10;
            }

            if (summaryTicksLeft > 0) {
                showSummary();
                summaryTicksLeft--;
            } else {
                showIdle();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}