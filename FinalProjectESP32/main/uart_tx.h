#ifndef UART_TX_H
#define UART_TX_H

#include <stdint.h>
#include "driver/uart.h"
#include "uart_packet.h"   /* reuses PACKET_START1, PACKET_START2, PACKET_END */

/*
 * Command packet sent FROM ESP32 TO MCXC444 (5 bytes):
 *
 *   [0]  0xAA        PACKET_START1
 *   [1]  0x55        PACKET_START2
 *   [2]  cmd         0x01 = LED ON  |  0x00 = LED OFF
 *   [3]  checksum    cmd ^ 0xFF
 *   [4]  0xBB        PACKET_END
 *
 * Matches MCXC444 uart_rx.h RX_PKT_LEN / RX_CHECKSUM exactly.
 */
#define TX_PKT_LEN          5U
#define TX_CMD_LED_ON       0x01U
#define TX_CMD_LED_OFF      0x00U
#define TX_CHECKSUM(cmd)    ((uint8_t)((cmd) ^ 0xFFU))

/*
 * UART_TX_Init()
 *   Enables GPIO16 as TX on the already-installed UART1 driver.
 *   Must be called AFTER UART_RX_Init().
 *
 * UART_TX_SendCmd(uint8_t cmd)
 *   Builds and sends the 5-byte packet for the given cmd byte.
 *   Call with TX_CMD_LED_ON or TX_CMD_LED_OFF.
 *   Thread-safe — uart_write_bytes() is internally protected.
 */
void UART_TX_Init(void);
void UART_TX_SendCmd(uint8_t cmd);

#endif /* UART_TX_H */
