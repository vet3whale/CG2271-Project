/*
 * uart_tx.h
 *
 *  Created on: 24 Mar 2026
 *      Author: User
 */

#ifndef UART_TX_H_
#define UART_TX_H_

#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "clock_config.h"
#include "uart_packet.h"

/* TX → PTD3, ALT3
 * RX → PTD2, ALT3
*/

#define UART_TX_PIN	22 /* PTE22 */
#define UART_RX_PIN 23 /* PTE23 */
#define MCXC_UART_BAUD 115200


void initUART2_RXTX(uint32_t baud_rate);
void vTxTask(void *pvParameters);

#endif /* UART_TX_H_ */
