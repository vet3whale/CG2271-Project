/*
 * uart_packet.h
 *
 *  Created on: 24 Mar 2026
 *      Author: User
 */

#ifndef UART_PACKET_H_
#define UART_PACKET_H_
#include <stdint.h>

#define PACKET_START1   0xAAU
#define PACKET_START2   0x55U
#define PACKET_END      0xBBU

/* MCXC to ESP32 TX packet */
/* [0] START1 [1] START2 [2] tap [3] focus [4] light_H [5] light_L
   [6] sound_H [7] sound_L [8] sound_triggered [9] env_condition
   [10] checksum XOR[2..9]  [11] END */
#define PACKET_LEN      12U

/* ESP32 to MCXC LED command packet */
/* [0] START1 [1] START2 [2] cmd [3] checksum [4] END */
#define RX_CMD_LED_ON   0x01U
#define RX_CMD_LED_OFF  0x00U
#define RX_CHECKSUM(cmd) ((uint8_t)(PACKET_START1 ^ PACKET_START2 ^ (cmd)))

/* ESP32 to MCXC Temperature packet */
/* [0] START1  [1] TEMP_START2  [2] temp_int  [3] temp_frac
   [4] checksum XOR[2..3]  [5] END */
#define TEMP_PKT_START2 0x56U
#define TEMP_PKT_LEN 6U
#define TEMP_PKT_CHECKSUM(ti, tf) ((uint8_t)((ti) ^ (tf)))

// Environment condition values (sent in byte [9] of TX packet)
#define ENV_UNKNOWN     0x00U
#define ENV_TOO_DARK    0x01U
#define ENV_TOO_LOUD    0x02U
#define ENV_TOO_HOT     0x03U
#define ENV_TOO_COLD    0x04U
#define ENV_GOOD        0x05U
#define ENV_MODRERATE   0x06U
#define ENV_POOR        0x07U

#endif /* UART_PACKET_H_ */
