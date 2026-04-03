#ifndef UART_RX_H_
#define UART_RX_H_

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "uart_packet.h" // reuses PACKET_START1, PACKET_START2, PACKET_END

/*
 * RX command packet sent FROM ESP32 TO MCXC444 (5 bytes):
 *
 *   [0]  0xAA        START1       (same markers as TX)
 *   [1]  0x55        START2
 *   [2]  cmd         0x01 = LED ON | 0x00 = LED OFF
 *   [3]  checksum    cmd ^ 0xFF
 *   [4]  0xBB        END
 *
 * PTE23 (ALT4 = UART2_RX) is already muxed by initUART2_TX().
 * initUART2_RX() MUST be called AFTER initUART2_TX().
 */

#define RX_PKT_LEN       5U
#define RX_CMD_LED_ON    0x01U
#define RX_CMD_LED_OFF   0x00U
#define RX_CHECKSUM(cmd) ((uint8_t)((cmd) ^ 0xFFU))

#define RX_TASK_STACK    128U

void initUART2_RX(void);
void vRXTask(void *pvParameters);

#endif /* UART_RX_H_ */
