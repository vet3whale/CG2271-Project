#include "uart_tx.h"
#include "shared_data.h"
#include "uart_packet.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "board.h"
#include "clock_config.h"
#include "fsl_debug_console.h"

static void build_packet(uint8_t tap, uint8_t on_off, uint8_t paused,
                         uint16_t light, uint16_t sound,
                         uint8_t triggered, uint8_t env_cond, uint8_t temp, uint8_t temp_frac, uint8_t hum, uint8_t hum_frac,
                         uint8_t *pkt)
{
    uint8_t chk = 0;
    pkt[0]  = PACKET_START1;
    pkt[1]  = PACKET_START2;
    pkt[2]  = tap;
    pkt[3]  = on_off;
    pkt[4]  = paused;
    pkt[5]  = (uint8_t)(light >> 8);
    pkt[6]  = (uint8_t)(light & 0xFF);
    pkt[7]  = (uint8_t)(sound >> 8);
    pkt[8]  = (uint8_t)(sound & 0xFF);
    pkt[9]  = triggered;
    pkt[10] = env_cond;
    pkt[11] = temp;
    pkt[12] = temp_frac;
    pkt[13] = hum;
	pkt[14] = hum_frac;
    for (uint8_t i = 2; i <= PACKET_LEN-3; i++) chk ^= pkt[i];  /* ← XOR[2..9] */
    pkt[15] = chk;
    pkt[16] = PACKET_END;
}

/* ── Private: blocking UART2 send ───────────────────────────────────────── */
static void uart2_send_blocking(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        while (!(UART2->S1 & UART_S1_TDRE_MASK)) {}
        UART2->D = buf[i];
    }
    while (!(UART2->S1 & UART_S1_TC_MASK)) {}
}

/* ── Public: UART2 init ─────────────────────────────────────────────────── */
void initUART2_RXTX(uint32_t baud_rate)
{
    /* 1. Clock gate UART2 and PORTE */
    SIM->SCGC4 |= SIM_SCGC4_UART2_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;

    /* 2. Disable TX/RX before configuring */
    UART2->C2 &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK);

    /* 3. PTE22 → UART2_TX (ALT4), PTE23 → UART2_RX (ALT4) */
    PORTE->PCR[UART_TX_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTE->PCR[UART_TX_PIN] |=  PORT_PCR_MUX(4);
    PORTE->PCR[UART_RX_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTE->PCR[UART_RX_PIN] |=  PORT_PCR_MUX(4);

    /* 4. Baud rate */
    uint32_t bus_clk = CLOCK_GetBusClkFreq();
    uint32_t sbr     = (bus_clk + (baud_rate * 8)) / (baud_rate * 16);
    UART2->BDH &= ~UART_BDH_SBR_MASK;
    UART2->BDH |=  (uint8_t)((sbr >> 8) & UART_BDH_SBR_MASK);
    UART2->BDL  =  (uint8_t)(sbr & 0xFF);

    /* 5. 8N1 */
    UART2->C1 &= ~(UART_C1_LOOPS_MASK | UART_C1_RSRC_MASK |
                   UART_C1_PE_MASK    | UART_C1_M_MASK);

    /* 6. Enable TX and RX */
    UART2->C2 |= (UART_C2_TE_MASK | UART_C2_RE_MASK);
}

void vTxTask(void *pvParameters)
{
    (void)pvParameters;
    uint8_t  packet[PACKET_LEN];
    uint8_t  tap = 0, on_off = 0, paused = 0, triggered = 0, env_cond = ENV_UNKNOWN, temp = 0, temp_frac = 0, hum = 0, hum_frac = 0;
    uint16_t light = 0, sound = 0;

    while (1) {
        /* Block up to 100ms for a notify from vEnvTask, or send periodically */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            tap       = (uint8_t)gAverageSensorData.tap_event;
            on_off    = (uint8_t)gAverageSensorData.on_off;
            paused = (uint8_t)gAverageSensorData.paused;
            light     = gSensorData.light_raw;
            sound     = gSensorData.sound_raw;
            triggered = gAverageSensorData.sound_triggered;
            env_cond  = gAverageSensorData.env_condition;
            temp = gAverageSensorData.temperature;
            temp_frac = gAverageSensorData.temp_frac;
            hum = gAverageSensorData.humidity;
            hum_frac = gAverageSensorData.hum_frac;
            if (tap) gAverageSensorData.tap_event = 0;
            xSemaphoreGive(gSensorMutex);
        }

        build_packet(tap, on_off, paused, light, sound, triggered, env_cond, temp, temp_frac, hum, hum_frac, packet);
        uart2_send_blocking(packet, PACKET_LEN);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
