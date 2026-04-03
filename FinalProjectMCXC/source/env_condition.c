#include "env_condition.h"
#include "shared_data.h"
#include "uart_packet.h"
#include "uart_tx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

void vEnvTask(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        // Block here until vRXTask signals that fresh temp data arrived
        xSemaphoreTake(gTempReadySem, portMAX_DELAY);

        uint16_t light, sound;
        int8_t   temp;
        uint8_t  condition = ENV_GOOD;

        // Snapshot all sensor data
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            light = gSensorData.light_raw;
            sound = gSensorData.sound_raw;
            temp  = gSensorData.temperature;

            uint8_t issues = 0;
            if (light > LIGHT_TOO_DARK_THRESHOLD) issues++;
            if (sound > SOUND_TOO_LOUD_THRESHOLD) issues++;
            if (temp  > TEMP_TOO_HOT_CELSIUS)     issues++;
            if (temp  < TEMP_TOO_COLD_CELSIUS)    issues++;

            if (issues >= 3) {
                condition = ENV_POOR;
            } else if (issues == 2) {
                condition = ENV_MODRERATE;
            } else if (issues == 1) {
                if      (light < LIGHT_TOO_DARK_THRESHOLD) condition = ENV_TOO_DARK;
                else if (sound > SOUND_TOO_LOUD_THRESHOLD) condition = ENV_TOO_LOUD;
                else if (temp  > TEMP_TOO_HOT_CELSIUS)     condition = ENV_TOO_HOT;
                else                                       condition = ENV_TOO_COLD;
            } else {
                condition = ENV_GOOD;
            }
            gSensorData.env_condition = condition;
            xSemaphoreGive(gSensorMutex);
        }

        // Trigger vTxTask to send the updated packet immediately
        if (xTxTaskHandle != NULL)
            xTaskNotifyGive(xTxTaskHandle);
    }
}
