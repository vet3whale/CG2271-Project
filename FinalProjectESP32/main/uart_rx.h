#ifndef UART_RX_H
#define UART_RX_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "uart_packet.h"

#define MCXC_UART_PORT    UART_NUM_1
#define MCXC_UART_BAUD    115200
#define MCXC_UART_RXD_PIN 17
#define MCXC_UART_TXD_PIN 16
#define MCXC_LED_PIN      26

void UART_RX_Init(void);
void vUartRxTask(void *pvParameters);

#endif /* UART_RX_H */
