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
        xSemaphoreTake(gTempReadySem, portMAX_DELAY);

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            uint16_t light = gSensorData.light_raw;
            uint16_t sound = gSensorData.sound_raw;
            int8_t   temp  = gSensorData.temperature;

            uint8_t score = 0;

            // High-impact: directly disrupts focus
            if (sound > SOUND_TOO_LOUD_THRESHOLD) score += 3;
            if (light > LIGHT_TOO_DARK_THRESHOLD) score += 2;

            // Low-impact: uncomfortable but manageable
            if (temp > TEMP_TOO_HOT_CELSIUS)      score += 1;
            if (temp < TEMP_TOO_COLD_CELSIUS)     score += 1;

            gSensorData.env_condition = (score == 0) ? ENV_GOOD : (score <= 2) ? ENV_MODERATE : ENV_POOR;
            g_color_blend = score*20;
            xSemaphoreGive(gSensorMutex);
        }

        if (xTxTaskHandle != NULL)
            xTaskNotifyGive(xTxTaskHandle);
    }
}
