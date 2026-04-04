#include <Arduino.h>
#include "uart_tx.h"
#include "uart_rx.h"        /* MCXC_UART_PORT, MCXC_UART_TXD_PIN */
#include "uart_packet.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ── Init ───────────────────────────────────────────────────────────────── */
void UART_TX_Init(void)
{
    uart_set_pin(MCXC_UART_PORT,
                 MCXC_UART_TXD_PIN,     /* TX — GPIO16 */
                 UART_PIN_NO_CHANGE,     /* RX — already set */
                 UART_PIN_NO_CHANGE,     /* RTS — not used */
                 UART_PIN_NO_CHANGE);    /* CTS — not used */
    Serial.println("[UART_TX] TX enabled on GPIO" + String(MCXC_UART_TXD_PIN));
}

/* ── LED command packet (5 bytes) ───────────────────────────────────────── */
void UART_TX_SendCmd(uint8_t cmd)
{
    uint8_t pkt[TX_PKT_LEN] = {
        PACKET_START1,
        PACKET_START2,
        cmd,
        TX_CHECKSUM(cmd),
        PACKET_END
    };

    int written = uart_write_bytes(MCXC_UART_PORT, (const char *)pkt, TX_PKT_LEN);
}

/* ── Temperature packet (6 bytes) ───────────────────────────────────────── */
void UART_TX_SendTemp(int8_t temp_int, uint8_t temp_frac, int8_t hum_int, uint8_t hum_frac) {
    uint8_t pkt[TEMP_PKT_LEN] = {
        PACKET_START1,
        TEMP_PKT_START2,
        (uint8_t)temp_int,
        temp_frac,
        (uint8_t) hum_int,
        hum_frac,
        TEMP_PKT_CHECKSUM(temp_int, temp_frac)^TEMP_PKT_CHECKSUM(hum_int, hum_frac),
        PACKET_END
    };

    int written = uart_write_bytes(MCXC_UART_PORT, (const char *)pkt, TEMP_PKT_LEN);
    if (written != TEMP_PKT_LEN) 
        Serial.println("[UART_TX] Warning: temp packet write incomplete");
    
}