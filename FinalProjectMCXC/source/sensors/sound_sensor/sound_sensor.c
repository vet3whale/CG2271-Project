#include <stdio.h>
#include <stdbool.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sound_sensor.h"
#include "../../shared_data/shared_data.h"

void sound_sensor_hw_init(void) {
	SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
	SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
	PORTB->PCR[0] = 0U; // PTB0, ALT0 for ADC0_SE8

	NVIC_DisableIRQ(ADC0_IRQn);

	// clock divide is 8, long sample time, 16-bit conversion
	ADC0->CFG1 = ADC_CFG1_ADIV(3) | ADC_CFG1_ADLSMP_MASK | ADC_CFG1_MODE(3);
	ADC0->SC2 = ADC_SC2_REFSEL(1); // alternate reference pair
	ADC0->SC3 = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3) | ADC_SC3_CAL_MASK; // enable hardware averaging, average 32 samples,

	while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK));

	if (ADC0->SC3 & ADC_SC3_CALF_MASK) {
		ADC0->SC3 |= ADC_SC3_CALF_MASK; // if calibration failed, CALF = 1, rewrite bit to correct it
	} else {
		uint16_t cal;

		// Plus-side gain: sum CLP registers, half, set MSB
		cal = (uint16_t) (ADC0->CLPD + ADC0->CLPS + ADC0->CLP4 +
		ADC0->CLP3 + ADC0->CLP2 + ADC0->CLP1 + ADC0->CLP0);
		ADC0->PG = (uint32_t) ((cal >> 1U) | 0x8000U);

		// Minus-side gain: sum CLM registers, half, set MSB
		cal = (uint16_t) (ADC0->CLMD + ADC0->CLMS + ADC0->CLM4 +
		ADC0->CLM3 + ADC0->CLM2 + ADC0->CLM1 + ADC0->CLM0);
		ADC0->MG = (uint32_t) ((cal >> 1U) | 0x8000U);
	}

	NVIC_EnableIRQ(ADC0_IRQn);
}

void sound_sensor_init(void) {
	NVIC_DisableIRQ(ADC0_IRQn);
	NVIC_ClearPendingIRQ(ADC0_IRQn);
	ADC0->SC1[0] = ADC_SC1_ADCH(0x1FU);
	ADC0->CFG1 = ADC_CFG1_ADIV(1) | ADC_CFG1_ADLSMP_MASK | ADC_CFG1_MODE(3);
	ADC0->SC2 = ADC_SC2_REFSEL(1);
	ADC0->SC3 = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3);
}

uint16_t sound_sensor_read(void) {
	ADC0->SC1[0] = ADC_SC1_ADCH(SOUND_ADC_CHANNEL);
	while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0U);
	return (uint16_t) ADC0->R[0];
}

void vSoundTask(void *pvParameters) {
	(void) pvParameters;

	uint32_t sum = 0U;
	uint32_t count = 0U;
	uint16_t sample = 0U;
	uint16_t baseline = 0U;

	static uint16_t triggerCount30s = 0U;
	static TickType_t windowStart = 0;
	static TickType_t lastEventTick = 0;

	const TickType_t windowTicks = pdMS_TO_TICKS(30000);
	const TickType_t rearmTicks = pdMS_TO_TICKS(300); // debounce condition

	// one time calibration
	xSemaphoreTake(gADCMutex, portMAX_DELAY);
	sound_sensor_hw_init();
	xSemaphoreGive(gADCMutex);

	// baseline sampling
	for (uint32_t t = 0U; t < CAL_TIME_MS; t += SAMPLE_DELAY_MS)
	{
		xSemaphoreTake(gADCMutex, portMAX_DELAY);
		sound_sensor_init(); // set ADC
		sample = sound_sensor_read(); // read
		NVIC_EnableIRQ(ADC0_IRQn);
		xSemaphoreGive(gADCMutex);

		sum += sample;
		count++;
		vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
	}

	baseline = (uint16_t) (sum / count);
	windowStart = xTaskGetTickCount();

	// main while loop in task
	while (1) {
		uint16_t peak = 0U, trough = 0xFFFFU, aboveCount = 0U;

		xSemaphoreTake(gADCMutex, portMAX_DELAY);
		sound_sensor_init();
		for (uint32_t i = 0U; i < PEAK_SAMPLE_COUNT; i++) {
			sample = sound_sensor_read();

			if (sample > peak) peak = sample;
			if (sample < trough) trough = sample;

			uint16_t dev = (sample > baseline) ? (sample - baseline) : 0;
			if (dev > TRIGGER_DELTA)
				aboveCount++;
		}
		NVIC_EnableIRQ(ADC0_IRQn);
		xSemaphoreGive(gADCMutex);

		uint16_t swing = (peak > trough) ? (peak - trough) : 0U;
		bool issustained = (aboveCount >= PEAK_SAMPLE_COUNT / 10U);
		bool isloud = (swing > TRIGGER_DELTA);
		bool triggered = isloud && issustained;

		TickType_t now = xTaskGetTickCount();

		// Count only a fresh trigger edge, with debounce cooldown
		if (triggered && (now - lastEventTick >= rearmTicks)) {
			triggerCount30s++;
			lastEventTick = now;
		}

		// Reset window counter every 30 s
		if ((now - windowStart) >= windowTicks) {
			triggerCount30s = 0U;
			windowStart = now;
		}

		if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
			gSensorData.sound = peak;
			gSensorData.sound_triggered = triggered ? 1U : 0U;
			soundTriggerCount30s = triggerCount30s;
			xSemaphoreGive(gSensorMutex);
		}

		vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
	}
}
