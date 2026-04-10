#include <Arduino.h>
#include "uart_tx.h"
#include "uart_rx.h"
#include "uart_packet.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void UART_TX_Init(void) {
  uart_set_pin(MCXC_UART_PORT,
               MCXC_UART_TXD_PIN,   /* TX — GPIO16 */
               UART_PIN_NO_CHANGE,  /* RX — already set */
               UART_PIN_NO_CHANGE,  /* RTS — not used */
               UART_PIN_NO_CHANGE); /* CTS — not used */
}

// for sending a cmd packet to mcxc
void UART_TX_SendCmd(uint8_t cmd) {
  uint8_t pkt[TX_PKT_LEN] = {
    PACKET_START1,
    PACKET_START2,
    cmd,
    TX_CHECKSUM(cmd),
    PACKET_END
  };
  uart_write_bytes(MCXC_UART_PORT, (const char *)pkt, TX_PKT_LEN);
}

// for sending 
void UART_TX_SendTemp(int8_t temp_int, uint8_t temp_frac, int8_t hum_int, uint8_t hum_frac) {
  uint8_t pkt[TEMP_PKT_LEN] = {
    PACKET_START1,
    TEMP_PKT_START2,
    (uint8_t)temp_int,
    temp_frac,
    (uint8_t)hum_int,
    hum_frac,
    TEMP_PKT_CHECKSUM(temp_int, temp_frac) ^ TEMP_PKT_CHECKSUM(hum_int, hum_frac),
    PACKET_END
  };
  uart_write_bytes(MCXC_UART_PORT, (const char *)pkt, TEMP_PKT_LEN);
}