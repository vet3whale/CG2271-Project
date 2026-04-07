<<<<<<< HEAD
=======
// oled_display.cpp
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
#include "oled_display.h"
#include "shared_data.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
<<<<<<< HEAD
#define OLED_RESET     -1    // no reset pin
#define OLED_ADDRESS  0x3C   // most SSD1306 modules use 0x3C
=======
#define OLED_RESET     -1
#define OLED_ADDRESS  0x3C
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ── Session timer ───────────────────────────────────────────────────────── */
static unsigned long sSessionStart = 0;
static unsigned long sPausedAt     = 0;
static unsigned long sTotalPaused  = 0;
static bool          sWasRunning   = false;

<<<<<<< HEAD
=======
/* ── Session stats accumulators ──────────────────────────────────────────── */
static float    sTempSum      = 0.0f;
static float    sHumSum       = 0.0f;
static uint32_t sSampleCount  = 0;
static float    sAvgTemp      = 0.0f;
static float    sAvgHum       = 0.0f;
static unsigned long sFinalElapsed = 0;  // saved when session ends

>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
static String formatTime(unsigned long seconds) {
    unsigned long h = seconds / 3600;
    unsigned long m = (seconds % 3600) / 60;
    unsigned long s = seconds % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
    return String(buf);
}

/* ── Display states ──────────────────────────────────────────────────────── */
static void showRunning(unsigned long elapsed) {
    display.clearDisplay();
<<<<<<< HEAD

=======
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 0);
    display.println("STUDY SESSION");
<<<<<<< HEAD

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Timer — big font
    display.setTextSize(2);
    display.setCursor(10, 22);
    display.println(formatTime(elapsed));

    display.setTextSize(1);
    display.setCursor(45, 50);
    display.println("RUNNING");

=======
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
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
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
    display.display();
}

static void showPaused(unsigned long elapsed) {
    display.clearDisplay();
<<<<<<< HEAD

=======
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 0);
    display.println("STUDY SESSION");
<<<<<<< HEAD

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Pause symbol — two rectangles
    display.fillRect(44, 20, 10, 24, SSD1306_WHITE);
    display.fillRect(60, 20, 10, 24, SSD1306_WHITE);

    // Frozen timer
    display.setTextSize(1);
    display.setCursor(28, 50);
    display.print("PAUSED  ");
    display.print(formatTime(elapsed));

=======
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    display.fillRect(44, 14, 10, 20, SSD1306_WHITE);
    display.fillRect(60, 14, 10, 20, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(28, 38);
    display.print("PAUSED  ");
    display.print(formatTime(elapsed));

    // Live avg temp & humidity while paused
    display.setCursor(0, 50);
    display.printf("T:%.1fC  H:%.1f%%", sAvgTemp, sAvgHum);
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

>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
    display.display();
}

static void showIdle() {
    display.clearDisplay();
<<<<<<< HEAD

=======
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 10);
    display.println("No active session");
<<<<<<< HEAD

    display.setCursor(22, 30);
    display.println("Tap to start!");

    display.drawRect(0, 0, 127, 63, SSD1306_WHITE);

    display.display();
}

/* ── Init ────────────────────────────────────────────────────────────────── */
void OLED_Init() {
    Wire.begin(8, 9);  // SDA=GPIO8, SCL=GPIO9

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("[OLED] Failed to initialise — check wiring");
        return;
=======
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
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(25, 28);
    display.println("Study Coach");
    display.display();
<<<<<<< HEAD

    Serial.println("[OLED] Initialised");
=======
    Serial.println("[OLED] Splash screen displayed");
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
    delay(1500);
}

/* ── Task ────────────────────────────────────────────────────────────────── */
void vOLEDTask(void *pvParameters) {
    (void)pvParameters;

    uint8_t onOff  = 0;
    uint8_t paused = 0;
<<<<<<< HEAD

    while (1) {
        // Read shared state under mutex
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            onOff  = gSensorData.on_off;
            paused = gSensorData.paused;
            xSemaphoreGive(gSensorMutex);
=======
    float   temp   = 0.0f;
    float   hum    = 0.0f;

    // Summary screen duration: show for 5 seconds (10 × 500ms ticks)
    uint8_t summaryTicksLeft = 0;

    Serial.println("[OLED] Task started");

    while (1) {
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            onOff  = gSensorData.on_off;
            paused = gSensorData.paused;
            temp   = gSensorData.esp_temp;
            hum    = gSensorData.esp_humidity;
            xSemaphoreGive(gSensorMutex);
        } else {
            Serial.println("[OLED] Warning: could not take sensor mutex");
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
        }

        unsigned long now = millis();

        if (onOff == 1 && paused == 0) {
<<<<<<< HEAD
            // Session is RUNNING
            if (!sWasRunning) {
                if (sSessionStart == 0) {
                    // Fresh start
                    sSessionStart = now;
                    sTotalPaused  = 0;
                } else {
                    // Resumed from pause
                    sTotalPaused += (now - sPausedAt);
                }
                sWasRunning = true;
            }
=======
            // ── RUNNING ──
            summaryTicksLeft = 0;

            if (!sWasRunning) {
                if (sSessionStart == 0) {
                    // Fresh start — reset all accumulators
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

            // Accumulate sample
            if (temp > 0.0f && hum > 0.0f) {
                sTempSum += temp;
                sHumSum  += hum;
                sSampleCount++;
            }

            // Update running averages
            if (sSampleCount > 0) {
                sAvgTemp = sTempSum / sSampleCount;
                sAvgHum  = sHumSum  / sSampleCount;
            }

>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
            unsigned long elapsed = (now - sSessionStart - sTotalPaused) / 1000;
            showRunning(elapsed);

        } else if (onOff == 1 && paused == 1) {
<<<<<<< HEAD
            // Session is PAUSED
            if (sWasRunning) {
                sPausedAt   = now;
                sWasRunning = false;
            }
=======
            // ── PAUSED ──
            summaryTicksLeft = 0;

            if (sWasRunning) {
                sPausedAt   = now;
                sWasRunning = false;
                Serial.println("[OLED] Session paused");
            }

>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
            unsigned long elapsed = (sPausedAt - sSessionStart - sTotalPaused) / 1000;
            showPaused(elapsed);

        } else {
<<<<<<< HEAD
            // onOff == 0 — session ended or not yet started
            if (sWasRunning) {
                // Session just stopped — reset timer state for next session
                sWasRunning   = false;
                sSessionStart = 0;
                sTotalPaused  = 0;
            }
            showIdle();
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // refresh every 500 ms
    }
}
=======
            // ── OFF / IDLE ──
            if (sWasRunning || sSessionStart != 0) {
                // Session just ended — compute final stats and show summary
                sFinalElapsed = (now - sSessionStart - sTotalPaused) / 1000;
                if (sSampleCount > 0) {
                    sAvgTemp = sTempSum / sSampleCount;
                    sAvgHum  = sHumSum  / sSampleCount;
                }
                Serial.printf("[OLED] Session ended — elapsed=%lus avgT=%.1f avgH=%.1f samples=%lu\n",
                              sFinalElapsed, sAvgTemp, sAvgHum, sSampleCount);

                // Reset session state
                sWasRunning   = false;
                sSessionStart = 0;
                sTotalPaused  = 0;

                summaryTicksLeft = 10; // show summary for ~5 seconds
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
>>>>>>> 42cdd8fb460afe9c2fd304622b9f6caef5709db9
