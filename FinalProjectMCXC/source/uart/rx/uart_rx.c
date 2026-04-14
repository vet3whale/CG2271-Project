#include "uart_rx.h"
#include "../../shared_data/shared_data.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "queue.h"

QueueHandle_t xRxQueue = NULL;

void initUART2_RX_Interrupts(void) {
    // create the Queue to hold incoming bytes
    xRxQueue = xQueueCreate(16, sizeof(uint8_t));

    // enable the Interrupt for rx, set RIE bit in UART2_C2 register
    UART2->C2 |= UART_C2_RIE_MASK;

    // enable IRQ in the NVIC
    NVIC_SetPriority(UART2_FLEXIO_IRQn, 2);
    NVIC_EnableIRQ(UART2_FLEXIO_IRQn);
}

void UART2_FLEXIO_IRQHandler(void) {
    uint8_t data;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // check if the Receive Data Register Full flag is set
    if (UART2->S1 & UART_S1_RDRF_MASK) {
        data = UART2->D; // read data
        xQueueSendFromISR(xRxQueue, &data, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vRXTask(void *pvParameters) {
    (void)pvParameters;
    uint8_t b, start2;

    while (1) {
        xQueueReceive(xRxQueue, &b, portMAX_DELAY); // wait for queue, this task is dormant, till rx received
        if (b != PACKET_START1) continue;

        // read start2 from queue
        if (xQueueReceive(xRxQueue, &start2, pdMS_TO_TICKS(50)) != pdTRUE) continue;

        if (start2 == PACKET_START2) {
            uint8_t cmd, chk, end;
            if (xQueueReceive(xRxQueue, &cmd, pdMS_TO_TICKS(50)) != pdTRUE) continue;
            if (xQueueReceive(xRxQueue, &chk, pdMS_TO_TICKS(50)) != pdTRUE) continue;
            if (xQueueReceive(xRxQueue, &end, pdMS_TO_TICKS(50)) != pdTRUE) continue;

            if (chk == RX_CHECKSUM(cmd) && end == PACKET_END) {
                led_state = (cmd == RX_CMD_LED_ON) ? 1 : 0;
            }
        }
        else if (start2 == TEMP_PKT_START2) {
            int8_t temp_int, hum_int;
            uint8_t temp_frac, hum_frac, chk, end;

            if (xQueueReceive(xRxQueue, (uint8_t*)&temp_int, pdMS_TO_TICKS(50)) != pdTRUE) continue;
            if (xQueueReceive(xRxQueue, &temp_frac, pdMS_TO_TICKS(50)) != pdTRUE) continue;
            if (xQueueReceive(xRxQueue, (uint8_t*)&hum_int, pdMS_TO_TICKS(50)) != pdTRUE) continue;
            if (xQueueReceive(xRxQueue, &hum_frac, pdMS_TO_TICKS(50)) != pdTRUE) continue;
            if (xQueueReceive(xRxQueue, &chk, pdMS_TO_TICKS(50)) != pdTRUE) continue;
            if (xQueueReceive(xRxQueue, &end, pdMS_TO_TICKS(50)) != pdTRUE) continue;

            if (chk == (TEMP_PKT_CHECKSUM(temp_int, temp_frac)^TEMP_PKT_CHECKSUM(hum_int, hum_frac)) && end == PACKET_END) {
                if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    gSensorData.temperature = temp_int;
                    gSensorData.temp_frac   = temp_frac;
                    gSensorData.humidity    = hum_int;
					gSensorData.hum_frac    = hum_frac;
                    xSemaphoreGive(gSensorMutex);
                }
                xSemaphoreGive(gTempReadySem);
            }
        }
    }
}
