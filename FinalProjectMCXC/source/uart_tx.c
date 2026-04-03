#include "uart_tx.h"
#include "shared_data.h"
#include "uart_packet.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "board.h"
#include "clock_config.h"
#include "fsl_debug_console.h"

/* ── Private: build 12-byte TX packet ───────────────────────────────────── */
static void build_packet(uint8_t tap, uint8_t focus,
                         uint16_t light, uint16_t sound,
                         uint8_t triggered, uint8_t env_cond, uint8_t temp,  /* ← ADDED */
                         uint8_t *pkt)
{
    uint8_t chk = 0;
    pkt[0]  = PACKET_START1;
    pkt[1]  = PACKET_START2;
    pkt[2]  = tap;
    pkt[3]  = focus;
    pkt[4]  = (uint8_t)(light >> 8);
    pkt[5]  = (uint8_t)(light & 0xFF);
    pkt[6]  = (uint8_t)(sound >> 8);
    pkt[7]  = (uint8_t)(sound & 0xFF);
    pkt[8]  = triggered;
    pkt[9]  = env_cond;                        /* ← env_condition byte */
    pkt[10] = temp;
    for (uint8_t i = 2; i <= 10; i++) chk ^= pkt[i];  /* ← XOR[2..9] */
    pkt[11] = chk;
    pkt[12] = PACKET_END;
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

/* ── Public: TX task ────────────────────────────────────────────────────── */
void vTxTask(void *pvParameters)
{
    (void)pvParameters;
    uint8_t  packet[PACKET_LEN];
    uint8_t  tap = 0, on_off = 0, triggered = 0, env_cond = ENV_UNKNOWN, temp = 0;
    uint16_t light = 0, sound = 0;

    while (1) {
        /* Block up to 100ms for a notify from vEnvTask, or send periodically */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            tap       = (uint8_t)gSensorData.tap_event;
            on_off    = (uint8_t)gSensorData.on_off;
            light     = gSensorData.light_raw;
            sound     = gSensorData.sound_raw;
            triggered = gSensorData.sound_triggered;
            env_cond  = gSensorData.env_condition;
            temp = gSensorData.temperature;
            if (tap) gSensorData.tap_event = 0;
            xSemaphoreGive(gSensorMutex);
        }

        build_packet(tap, on_off, light, sound, triggered, env_cond, temp, packet); /* ← PASS env_cond */
        uart2_send_blocking(packet, PACKET_LEN);
    }
}
