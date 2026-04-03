#ifndef UART_PACKET_H
#define UART_PACKET_H

#include <stdint.h>

#define PACKET_START1   0xAA
#define PACKET_START2   0x55
#define PACKET_END      0xBB
#define PACKET_LEN      11

<<<<<<< Updated upstream
/*  Byte layout (11 bytes total):
 *  [0]  0xAA              start marker 1
 *  [1]  0x55              start marker 2
 *  [2]  tap_event         (uint8_t)
 *  [3]  focus_mode        (uint8_t)
 *  [4]  light_raw_H       (uint8_t)
 *  [5]  light_raw_L       (uint8_t)
 *  [6]  sound_raw_H       (uint8_t)
 *  [7]  sound_raw_L       (uint8_t)
 *  [8]  sound_triggered   (uint8_t)
 *  [9]  checksum          XOR of bytes [2..8]
 *  [10] 0xBB              end marker
 */
=======
/* ── MCXC → ESP32 RX packet (12 bytes) ──────────────────────────────────
 * [0]  START1
 * [1]  START2 (0x55)
 * [2]  tap_event
 * [3]  on_off
 * [4]  paused
 * [5]  light_raw_H
 * [6]  light_raw_L
 * [7]  sound_raw_H
 * [8]  sound_raw_L
 * [9]  sound_triggered
 * [10]  env_condition
 * [11] temp
 * [12] checksum XOR[2..9]
 * [13] END
 */
#define PACKET_LEN      14U
>>>>>>> Stashed changes

#endif /* UART_PACKET_H */
