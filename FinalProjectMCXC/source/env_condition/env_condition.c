#include "env_condition.h"
#include "../shared_data/shared_data.h"
#include "../uart/uart_packet.h"
#include "../uart/tx/uart_tx.h"
#include "../actuators/buzzer/buzzer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdbool.h>

#define FILTER_SHIFT 4   // EMA factor = 1/16

static void update_temp_average(int8_t new_temp_int, uint8_t new_temp_frac) {
	static bool initialized = false;
	static int32_t avg_temp_x10 = 0;   // temperature in tenths

	int32_t new_temp_x10 = ((int32_t) new_temp_int * 10)
			+ (int32_t) new_temp_frac;

	if (!initialized) {
		avg_temp_x10 = new_temp_x10;
		initialized = true;
	} else {
		avg_temp_x10 = avg_temp_x10 - (avg_temp_x10 >> FILTER_SHIFT)
				+ (new_temp_x10 >> FILTER_SHIFT);
	}

	gAverageSensorData.temperature = (int8_t) (avg_temp_x10 / 10);
	gAverageSensorData.temp_frac = (uint8_t) (avg_temp_x10 % 10);
}

static void update_humidity_average(uint8_t new_hum_int, uint8_t new_hum_frac) {
	static bool initialized = false;
	static uint32_t avg_hum_x10 = 0;   // humidity in tenths

	uint32_t new_hum_x10 = ((uint32_t) new_hum_int * 10U)
			+ (uint32_t) new_hum_frac;

	if (!initialized) {
		avg_hum_x10 = new_hum_x10;
		initialized = true;
	} else {
		avg_hum_x10 = avg_hum_x10 - (avg_hum_x10 >> FILTER_SHIFT)
				+ (new_hum_x10 >> FILTER_SHIFT);
	}

	gAverageSensorData.humidity = (uint8_t) (avg_hum_x10 / 10U);
	gAverageSensorData.hum_frac = (uint8_t) (avg_hum_x10 % 10U);
}

void vEnvTask(void *pvParameters) {
	(void) pvParameters;

	while (1) {
		xSemaphoreTake(gTempReadySem, portMAX_DELAY);

		if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
			uint16_t light = gSensorData.light;
			uint16_t sound = gSensorData.sound;

			int8_t temp_int = gSensorData.temperature;
			uint8_t temp_frac = gSensorData.temp_frac;

			uint8_t hum_int = gSensorData.humidity;
			uint8_t hum_frac = gSensorData.hum_frac;

			gAverageSensorData.light = gAverageSensorData.light
					- (gAverageSensorData.light >> FILTER_SHIFT)
					+ (light >> FILTER_SHIFT);

			gAverageSensorData.sound = gAverageSensorData.sound
					- (gAverageSensorData.sound >> FILTER_SHIFT)
					+ (sound >> FILTER_SHIFT);

			update_temp_average(temp_int, temp_frac);
			update_humidity_average(hum_int, hum_frac);

			uint8_t score = 0;

			// use averaged temperature for condition
			int8_t avg_temp = gAverageSensorData.temperature;

			if (soundTriggerCount30s >= NOISY_TRIGGER_COUNT_30S)
				score += 3;
			if (light > LIGHT_TOO_DARK_THRESHOLD)
				score += 2;
			if (avg_temp > TEMP_TOO_HOT_CELSIUS)
				score += 1;
			if (avg_temp < TEMP_TOO_COLD_CELSIUS)
				score += 1;

			gSensorData.env_condition = (score == 0) ? ENV_GOOD :
										(score <= 3) ? ENV_MODERATE :
										ENV_POOR;
			if (gSensorData.env_condition == ENV_POOR && gSensorData.on_off
					&& !gSensorData.paused)
				gBuzzerRequest = BUZZ_LONG;

			if (score == 0)
				g_color_blend = 0;
			else if (score <= 2)
				g_color_blend = 40;
			else if (score <= 3)
				g_color_blend = 80;
			else
				g_color_blend = 100;

			gAverageSensorData.tap_event = gSensorData.tap_event;
			gAverageSensorData.on_off = gSensorData.on_off;
			gAverageSensorData.paused = gSensorData.paused;
			gAverageSensorData.sound_triggered = gSensorData.sound_triggered;
			gAverageSensorData.env_condition = gSensorData.env_condition;

			if (gSensorData.tap_event) {
				gSensorData.tap_event = 0;
			}

			xSemaphoreGive(gSensorMutex);
		}

		if (xTxTaskHandle != NULL) {
			xTaskNotifyGive(xTxTaskHandle);
		}
	}
}
