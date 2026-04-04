#include <Arduino.h>
#include "uart_rx.h"
#include "uart_tx.h"          /* UART_TX_SendCmd, TX_CMD_LED_ON/OFF */
#include "shared_data.h"
#include "uart_packet.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ── Private helpers ─────────────────────────────────────────────────────── */

static const char* envConditionStr(uint8_t cond)
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

/*
 * validateChecksum()
 * XOR of bytes [2..8] must equal byte [9]
 * Matches build_packet() on MCXC444 side exactly
 */
static bool validateChecksum(uint8_t *pkt) {
    uint8_t chk = 0;
    for (uint8_t i = 2; i <= PACKET_LEN-3; i++) chk ^= pkt[i];
    return chk == pkt[PACKET_LEN-2];
}

static uint8_t  sPrevOnOff = 0;
static uint8_t  sPrevPaused = 0;
static uint32_t s_session_start_ms = 0;
static uint32_t s_total_focused_ms = 0;
static bool s_is_timing = false;

/*
 * parseAndStore()
 * Reconstructs uint16 from H and L bytes
 * Flashes LED on valid packet
 * Writes to gSensorData under mutex
 */
static void parseAndStore(uint8_t *pkt) {
    uint8_t  tap       = pkt[2];
    uint8_t  on_off     = pkt[3];
    uint8_t  paused     = pkt[4];
    uint16_t light     = ((uint16_t)pkt[5] << 8) | pkt[6];
    uint16_t sound     = ((uint16_t)pkt[7] << 8) | pkt[8];
    uint8_t  triggered = pkt[9];
    uint8_t env_cond  = pkt[10];
    uint8_t temp = pkt[11];
    uint8_t temp_frac = pkt[12];
    uint8_t hum = pkt[13];
    uint8_t hum_frac = pkt[14];

    UART_TX_SendCmd(on_off == 1 ? TX_CMD_LED_ON : TX_CMD_LED_OFF);
    if (on_off) {
        Serial.println("[UART] Valid packet received:");
        Serial.print("  tap_event:       "); Serial.println(tap);
        Serial.print("  on_off_mode:     "); Serial.println(on_off);
        Serial.print("  paused:          "); Serial.println(paused);
        Serial.print("  avg. light_raw:  "); Serial.println(light);
        Serial.print("  avg sound_raw:   "); Serial.println(sound);
        Serial.print("  sound_triggered: "); Serial.println(triggered);
        Serial.print("  env_cond:        "); Serial.println(envConditionStr(env_cond));
        Serial.print("  avg. temp:       "); Serial.print(temp); Serial.print("."); Serial.print(temp_frac); Serial.println("C");
        Serial.print("  avg. hum:        "); Serial.print(hum); Serial.print("."); Serial.print(hum_frac); Serial.println("%");
    }

    // 1. Detect START: on_off 0 -> 1
    if (sPrevOnOff == 0 && on_off == 1) {
        s_total_focused_ms = 0; // Reset timer for new session
        if (paused == 0) {
            s_session_start_ms = millis();
            s_is_timing = true;
            Serial.println("[UART] Session started.");
        }
    }
    // 2. Detect PAUSE: paused 0 -> 1 (while session is active)
    else if (on_off == 1 && sPrevPaused == 0 && paused == 1) {
        if (s_is_timing) {
            s_total_focused_ms += (millis() - s_session_start_ms);
            s_is_timing = false;
            Serial.println("[UART] Session paused.");
        }
    }
    // 3. Detect RESUME: paused 1 -> 0 (while session is active)
    else if (on_off == 1 && sPrevPaused == 1 && paused == 0) {
        if (!s_is_timing) {
            s_session_start_ms = millis();
            s_is_timing = true;
            Serial.println("[UART] Session resumed.");
        }
    }
    // 4. Detect STOP: on_off 1 -> 0
    else if (sPrevOnOff == 1 && on_off == 0) {
        if (s_is_timing) {
            s_total_focused_ms += (millis() - s_session_start_ms);
            s_is_timing = false;
        }
        
        if (gGeminiSemaphore != NULL) {
            xSemaphoreGive(gGeminiSemaphore);
            Serial.println("[UART] Session ended — Gemini semaphore given");
        }
    }

    sPrevOnOff = on_off;
    sPrevPaused = paused;

    if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        gSensorData.tap_event       = tap;
        gSensorData.on_off          = on_off;
        gSensorData.paused          = paused;
        gSensorData.light_raw       = light;
        gSensorData.temp            = temp;
        gSensorData.temp_frac       = temp_frac;
        gSensorData.hum             = hum;
        gSensorData.hum_frac        = hum_frac;
        gSensorData.sound_raw       = sound;
        gSensorData.sound_triggered = triggered;
        gSensorData.env_condition = env_cond;
        gSensorData.session_duration_sec = s_total_focused_ms / 1000;
        

        xSemaphoreGive(gSensorMutex);
    } else {
        Serial.println("[UART] Warning: could not take mutex — skipping write");
    }

}

void UART_RX_Init(void) {
    /* LED setup */
    pinMode(MCXC_LED_PIN, OUTPUT);
    digitalWrite(MCXC_LED_PIN, LOW);   // start OFF

    /*
     * Configure UART1 — receive only
     * TX pin set to UART_PIN_NO_CHANGE — ESP32 does not transmit
     */
    uart_config_t uart_config = {
        .baud_rate  = MCXC_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* RX buffer 256 bytes, no TX buffer needed */
    uart_driver_install(MCXC_UART_PORT, 256, 0, 0, NULL, 0);
    uart_param_config(MCXC_UART_PORT, &uart_config);

    /*
     * Only set RX pin — TX is UART_PIN_NO_CHANGE
     * This puts ESP32 in receive-only mode
     */
    uart_set_pin(MCXC_UART_PORT,
                 UART_PIN_NO_CHANGE,   // TX — not used
                 MCXC_UART_RXD_PIN,   // RX — GPIO17
                 UART_PIN_NO_CHANGE,   // RTS — not used
                 UART_PIN_NO_CHANGE);  // CTS — not used

    Serial.println("[UART] Receiver initialised — RX only mode");
}

void vUartRxTask(void *pvParameters) {
    (void)pvParameters;

    uint8_t buf[PACKET_LEN];
    uint8_t idx    = 0;
    bool    synced = false;
    uint8_t byte   = 0;

    Serial.println("[UART] Receive task started — waiting for packets...");

    while (1) {
        int received = uart_read_bytes(MCXC_UART_PORT,
                                       &byte, 1,
                                       pdMS_TO_TICKS(10));
        if (received <= 0) {
            continue;
        }

        /* ── Sync on start markers ──────────────────────────────────────── */
        if (!synced) {
            if (idx == 0 && byte == PACKET_START1) {
                buf[idx++] = byte;
            } else if (idx == 1 && byte == PACKET_START2) {
                buf[idx++] = byte;
                synced = true;
            } else if (idx == 1 && byte == PACKET_START1) {
                /* Previous 0xAA was not a real start — keep this one */
                idx = 1;
            } else {
                idx = 0;
            }
            continue;
        }

        buf[idx++] = byte;

        if (idx == PACKET_LEN) {
            idx    = 0;
            synced = false;

            /* Validate end marker */
            if (buf[PACKET_LEN - 1] != PACKET_END) {
                Serial.println("[UART] Error: bad end marker — dropped");
                continue;
            }

            /* Validate checksum */
            if (!validateChecksum(buf)) {
                Serial.println("[UART] Error: bad checksum — dropped");
                continue;
            }

            /* Valid — parse and store */
            parseAndStore(buf);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}