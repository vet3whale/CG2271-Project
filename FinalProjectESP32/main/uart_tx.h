#ifndef UART_TX_H
#define UART_TX_H
#include <stdint.h>
#include "driver/uart.h"
#include "uart_packet.h"

// esp to mcxc to send command
// [0] 0xAA [1] 0x55 [2] cmd [3] cmd^0xFF [4] 0xBB
#define TX_PKT_LEN 5U
#define TX_CMD_LED_ON 0x01U
#define TX_CMD_LED_OFF 0x00U
#define TX_CHECKSUM(cmd) ((uint8_t)((cmd) ^ 0xFFU))

void UART_TX_Init(void);
void UART_TX_SendCmd(uint8_t cmd);
void UART_TX_SendTemp(int8_t temp_int, uint8_t temp_frac, int8_t hum_int, uint8_t hum_frac);

#endif /* UART_TX_H */