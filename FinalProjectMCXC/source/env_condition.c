#include "env_condition.h"
#include "shared_data.h"
#include "uart_packet.h"
#include "uart_tx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define TEMP_PRECISION_SHIFT 8  // Scale by 256
#define FILTER_SHIFT 4          // Smoothing factor (1/16th)

void update_temp_average(int8_t new_temp_reading) {
	// 1. Scale the new reading to match our high-precision average
	int32_t scaled_new = (int32_t)new_temp_reading << TEMP_PRECISION_SHIFT;

	// 2. Apply the EMA formula
	// We use a temporary 32-bit variable to avoid overflow during calculation
	static int32_t long_term_avg = 0;

	// Initialize if it's the first run
	if (long_term_avg == 0)
		long_term_avg = scaled_new;

	long_term_avg = long_term_avg - (long_term_avg >> FILTER_SHIFT) + (scaled_new >> FILTER_SHIFT);

	// 3. Store back to your shared data
	// Shift back down to get the whole number for your logic
	gAverageSensorData.temperature = (int8_t)(long_term_avg >> TEMP_PRECISION_SHIFT);
	gAverageSensorData.temp_frac = (uint8_t)(long_term_avg & 0xFF);
}

void vEnvTask(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        xSemaphoreTake(gTempReadySem, portMAX_DELAY);

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            uint16_t light = gSensorData.light_raw;
            uint16_t sound = gSensorData.sound_raw;
            int8_t   temp  = gSensorData.temperature;

            gAverageSensorData.light_raw = gAverageSensorData.light_raw -
                                              (gAverageSensorData.light_raw >> FILTER_SHIFT) +
                                              (light >> FILTER_SHIFT);

			gAverageSensorData.sound_raw = gAverageSensorData.sound_raw -
										  (gAverageSensorData.sound_raw >> FILTER_SHIFT) +
										  (sound >> FILTER_SHIFT);
			update_temp_average(temp);

            uint8_t score = 0;

            // High-impact: directly disrupts focus
            if (sound > SOUND_TOO_LOUD_THRESHOLD) score += 3;
            if (light > LIGHT_TOO_DARK_THRESHOLD) score += 2;

            // Low-impact: uncomfortable but manageable
            if (temp > TEMP_TOO_HOT_CELSIUS)      score += 1;
            if (temp < TEMP_TOO_COLD_CELSIUS)     score += 1;

            gSensorData.env_condition = (score == 0) ? ENV_GOOD : (score <= 2) ? ENV_MODERATE : ENV_POOR;

            if (score == 0) g_color_blend = 0;
            else if (score <= 2) g_color_blend = 40;
            else if (score <= 3) g_color_blend = 70;
            else g_color_blend = 100;

            gAverageSensorData.tap_event = gSensorData.tap_event;
            gAverageSensorData.on_off = gSensorData.on_off;
            gAverageSensorData.paused = gSensorData.paused;
            gAverageSensorData.sound_triggered = gSensorData.sound_triggered;
            gAverageSensorData.temp_frac = gSensorData.temp_frac;
            gAverageSensorData.env_condition = gSensorData.env_condition;
            if (gSensorData.tap_event) gSensorData.tap_event = 0;

            xSemaphoreGive(gSensorMutex);
        }

        if (xTxTaskHandle != NULL)
            xTaskNotifyGive(xTxTaskHandle);
    }
}
