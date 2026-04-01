#include "uart_rx.h"
#include "shared_data.h"
#include "fsl_debug_console.h"
#include "board.h"


static uint8_t uart2_read_byte(void)
{
	while (!(UART2->S1 & UART_S1_RDRF_MASK));
    return (uint8_t)UART2->D;   /* reading D clears RDRF */
}

void vRXTask(void *pvParameters)
{
    (void)pvParameters;
    uint8_t b, start2;

    while (1) {
        // 1. Sync on START1
        do { b = uart2_read_byte(); } while (b != PACKET_START1);

        // 2. Read START2 — determines packet type
        start2 = uart2_read_byte();

        if (start2 == PACKET_START2) {
            /* ---- LED command packet (5 bytes total) ---- */
            uint8_t cmd = uart2_read_byte();
            uint8_t chk = uart2_read_byte();
            if (chk != RX_CHECKSUM(cmd))         continue;
            if (uart2_read_byte() != PACKET_END) continue;

            led_state = (cmd == RX_CMD_LED_ON) ? 1 : 0;
        }
        else if (start2 == TEMP_PKT_START2) {
            /* ---- Temperature packet (6 bytes total) ---- */
            int8_t  temp_int  = (int8_t)uart2_read_byte();
            uint8_t temp_frac = uart2_read_byte();
            uint8_t chk       = uart2_read_byte();
            if (chk != TEMP_PKT_CHECKSUM(temp_int, temp_frac)) continue;
            if (uart2_read_byte() != PACKET_END)               continue;

            if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gSensorData.temperature = temp_int;
                gSensorData.temp_frac   = temp_frac;
                xSemaphoreGive(gSensorMutex);
            }
            xSemaphoreGive(gTempReadySem);
        }
    }
}

