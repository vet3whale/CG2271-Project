#include "oled_display.h"
#include "shared_data.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1    // no reset pin
#define OLED_ADDRESS  0x3C   // most SSD1306 modules use 0x3C

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ── Session timer ───────────────────────────────────────────────────────── */
static unsigned long sSessionStart = 0;
static unsigned long sPausedAt     = 0;
static unsigned long sTotalPaused  = 0;
static bool          sWasRunning   = false;

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

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 0);
    display.println("STUDY SESSION");

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Timer — big font
    display.setTextSize(2);
    display.setCursor(10, 22);
    display.println(formatTime(elapsed));

    display.setTextSize(1);
    display.setCursor(45, 50);
    display.println("RUNNING");

    display.display();
}

static void showPaused(unsigned long elapsed) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 0);
    display.println("STUDY SESSION");

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Pause symbol — two rectangles
    display.fillRect(44, 20, 10, 24, SSD1306_WHITE);
    display.fillRect(60, 20, 10, 24, SSD1306_WHITE);

    // Frozen timer
    display.setTextSize(1);
    display.setCursor(28, 50);
    display.print("PAUSED  ");
    display.print(formatTime(elapsed));

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

/* ── Init ────────────────────────────────────────────────────────────────── */
void OLED_Init() {
    Wire.begin(6, 7);  // SDA=GPIO8, SCL=GPIO9

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("[OLED] Failed to initialise — check wiring");
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(25, 28);
    display.println("Study Coach");
    display.display();

    Serial.println("[OLED] Initialised");
    delay(1500);
}

/* ── Task ────────────────────────────────────────────────────────────────── */
void vOLEDTask(void *pvParameters) {
    (void)pvParameters;

    uint8_t onOff  = 0;
    uint8_t paused = 0;

    while (1) {
        // Read shared state under mutex
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            onOff  = gSensorData.on_off;
            paused = gSensorData.paused;
            xSemaphoreGive(gSensorMutex);
        }

        unsigned long now = millis();

        if (onOff == 1 && paused == 0) {
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
            unsigned long elapsed = (now - sSessionStart - sTotalPaused) / 1000;
            showRunning(elapsed);

        } else if (onOff == 1 && paused == 1) {
            // Session is PAUSED
            if (sWasRunning) {
                sPausedAt   = now;
                sWasRunning = false;
            }
            unsigned long elapsed = (sPausedAt - sSessionStart - sTotalPaused) / 1000;
            showPaused(elapsed);

        } else {
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
