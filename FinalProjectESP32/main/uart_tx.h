#ifndef UART_TX_H
#define UART_TX_H
#include <stdint.h>
#include "driver/uart.h"
#include "uart_packet.h"

/* ── ESP32 → MCXC LED command packet (5 bytes) ───────────────────────────
 * [0] 0xAA  [1] 0x55  [2] cmd  [3] cmd^0xFF  [4] 0xBB
 */
#define TX_PKT_LEN          5U
#define TX_CMD_LED_ON       0x01U
#define TX_CMD_LED_OFF      0x00U
#define TX_CHECKSUM(cmd)    ((uint8_t)((cmd) ^ 0xFFU))

/*
 * UART_TX_Init()        — enables GPIO16 as TX on already-installed UART1.
 *                          Must be called AFTER UART_RX_Init().
 *
 * UART_TX_SendCmd()     — sends 5-byte LED command packet.
 *
 * UART_TX_SendTemp()    — sends 6-byte temperature packet to MCXC.
 *                          Called by vDHTTask every 1000ms.
 */
void UART_TX_Init(void);
void UART_TX_SendCmd(uint8_t cmd);
void UART_TX_SendTemp(int8_t temp_int, uint8_t temp_frac);   /* NEW */

#endif /* UART_TX_H */