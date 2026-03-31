#include "uart_rx.h"
#include "shared_data.h"
#include "fsl_debug_console.h"
#include "MCXC444.h"


static uint8_t uart2_read_byte(void)
{
	while (!(UART2->S1 & UART_S1_RDRF_MASK));
    return (uint8_t)UART2->D;   /* reading D clears RDRF */
}

void vRXTask(void *pvParameters)
{
    (void)pvParameters;
    uint8_t b;

    while(1){
        // Sync: wait for START1
        do { b = uart2_read_byte(); } while (b != PACKET_START1);

        // START2
        if (uart2_read_byte() != PACKET_START2) continue;

        uint8_t cmd = uart2_read_byte();
        uint8_t chk = uart2_read_byte();
        if (chk != RX_CHECKSUM(cmd)) continue;

        if (uart2_read_byte() != PACKET_END) continue;

        led_state = (cmd == RX_CMD_LED_ON) ? 1 : 0;
    }
}
