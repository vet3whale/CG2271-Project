#ifndef UART_PACKET_H
#define UART_PACKET_H
#include <stdint.h>

#define PACKET_START1   0xAAU
#define PACKET_START2   0x55U
#define PACKET_END      0xBBU

/* ── MCXC → ESP32 RX packet (12 bytes) ──────────────────────────────────
 * [0]  START1
 * [1]  START2 (0x55)
 * [2]  tap_event
 * [3]  focus_mode
 * [4]  light_raw_H
 * [5]  light_raw_L
 * [6]  sound_raw_H
 * [7]  sound_raw_L
 * [8]  sound_triggered
 * [9]  env_condition
 * [10] temp
 * [11] checksum XOR[2..9]
 * [12] END
 */
#define PACKET_LEN      13U

/* ── ESP32 → MCXC Temperature packet (6 bytes, NEW) ─────────────────────
 * [0] START1
 * [1] TEMP_PKT_START2 (0x56)   ← differs from LED packet to disambiguate
 * [2] temp_int   (int8_t,  e.g. 26 for 26.7°C)
 * [3] temp_frac  (uint8_t, e.g.  7 for 26.7°C)
 * [4] checksum   XOR of [2] and [3]
 * [5] END
 */
#define TEMP_PKT_START2             0x56U
#define TEMP_PKT_LEN                6U
#define TEMP_PKT_CHECKSUM(ti, tf)   ((uint8_t)((uint8_t)(ti) ^ (tf)))

/* ── Environment condition values (received in byte [9] of RX packet) ── */
#define ENV_UNKNOWN     0x00U
#define ENV_TOO_DARK    0x01U
#define ENV_TOO_LOUD    0x02U
#define ENV_TOO_HOT     0x03U
#define ENV_TOO_COLD    0x04U
#define ENV_GOOD        0x05U
#define ENV_MODERATE    0x06U
#define ENV_POOR        0x07U

#endif /* UART_PACKET_H */