#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "peripherals.h"
#include "fsl_debug_console.h"
#include "shared_data.h"
#include "FreeRTOS.h"
#include "task.h"

#include "light_sensor.h"
#include "tap.h"
#include "led.h"
#include "buzzer.h"
#include "sound_sensor.h"
#include "uart_tx.h"
#include "uart_rx.h"
#include "env_condition.h"

// task priority levels
#define TAPTASK_PRIORITY 4
#define ENVTASK_PRIORITY 3
#define SOUNDTASK_PRIORITY 2
#define LIGHTTASK_PRIORITY 2
#define LEDTASK_PRIORITY 2
#define RXTASK_PRIORITY 2
#define TXTASK_PRIORITY 1
#define BUZZERTASK_PRIORITY 3

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

    gSensorMutex = xSemaphoreCreateMutex();
    gADCMutex = xSemaphoreCreateMutex();
    gTempReadySem = xSemaphoreCreateBinary();

    TAP_Init();
    led_init();
    buzzer_init();
    initUART2_RXTX(MCXC_UART_BAUD);
    initUART2_RX_Interrupts();      // Enable the Queue and Interrupts

    xTaskCreate(LIGHT_SENSOR_Task, "Light",  configMINIMAL_STACK_SIZE+128, NULL, LIGHTTASK_PRIORITY, NULL);
    xTaskCreate(vTapTask, "Tap", configMINIMAL_STACK_SIZE, NULL, TAPTASK_PRIORITY, NULL);
    xTaskCreate(vSoundTask, "Sound", configMINIMAL_STACK_SIZE, NULL, SOUNDTASK_PRIORITY, NULL);

    xTaskCreate(vLEDTask, "LED", configMINIMAL_STACK_SIZE, NULL, LEDTASK_PRIORITY, NULL);
    xTaskCreate(vBuzzerTask, "Buzzer", configMINIMAL_STACK_SIZE, NULL, BUZZERTASK_PRIORITY, NULL);

    xTaskCreate(vTxTask, "TXTask", configMINIMAL_STACK_SIZE+128, NULL, TXTASK_PRIORITY, NULL);
    xTaskCreate(vRXTask, "RXTask", configMINIMAL_STACK_SIZE, NULL, RXTASK_PRIORITY, NULL);

    xTaskCreate(vEnvTask, "EnvTask",configMINIMAL_STACK_SIZE+128, NULL, ENVTASK_PRIORITY, NULL);

    vTaskStartScheduler();
    while(1);
}
