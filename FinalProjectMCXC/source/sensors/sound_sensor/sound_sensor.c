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

 // Internal: ADC Self-Calibration
static void adc_calibrate(void){
	NVIC_DisableIRQ(ADC0_IRQn);
	uint16_t cal_var;

    ADC0->CFG1 &= ~ADC_CFG1_ADIV_MASK;
    ADC0->CFG1 |=  ADC_CFG1_ADIV(3);
    ADC0->CFG1 |=  ADC_CFG1_ADLSMP_MASK;
    ADC0->CFG1 &= ~ADC_CFG1_MODE_MASK;
    ADC0->CFG1 |=  ADC_CFG1_MODE(0b11);
    ADC0->CFG1 &= ~ADC_CFG1_ADICLK_MASK;
    ADC0->CFG1 |=  ADC_CFG1_ADICLK(0b00);

    ADC0->SC2 &= ~ADC_SC2_ADTRG_MASK;
    ADC0->SC2 &= ~ADC_SC2_REFSEL_MASK;
    ADC0->SC2 |=  ADC_SC2_REFSEL(0b01);

    ADC0->SC3 |=  ADC_SC3_AVGE_MASK;
    ADC0->SC3 &= ~ADC_SC3_AVGS_MASK;
    ADC0->SC3 |=  ADC_SC3_AVGS(0b11);

    ADC0->SC3 |= ADC_SC3_CAL_MASK;

    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0U) {}

    if ((ADC0->SC3 & ADC_SC3_CALF_MASK) != 0U)
    {
        ADC0->SC3 |= ADC_SC3_CALF_MASK;
        return;
    }

    cal_var  = 0U;
    cal_var += (uint16_t)ADC0->CLPD;
    cal_var += (uint16_t)ADC0->CLPS;
    cal_var += (uint16_t)ADC0->CLP4;
    cal_var += (uint16_t)ADC0->CLP3;
    cal_var += (uint16_t)ADC0->CLP2;
    cal_var += (uint16_t)ADC0->CLP1;
    cal_var += (uint16_t)ADC0->CLP0;
    cal_var >>= 1U;
    cal_var  |= 0x8000U;
    ADC0->PG  = (uint32_t)cal_var;

    cal_var  = 0U;
    cal_var += (uint16_t)ADC0->CLMD;
    cal_var += (uint16_t)ADC0->CLMS;
    cal_var += (uint16_t)ADC0->CLM4;
    cal_var += (uint16_t)ADC0->CLM3;
    cal_var += (uint16_t)ADC0->CLM2;
    cal_var += (uint16_t)ADC0->CLM1;
    cal_var += (uint16_t)ADC0->CLM0;
    cal_var >>= 1U;
    cal_var  |= 0x8000U;
    ADC0->MG  = (uint32_t)cal_var;

    // PRINTF("[ADC] Calibration OK\r\n");
    NVIC_EnableIRQ(ADC0_IRQn);
}

void sound_sensor_init(void){
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;

    PORTB->PCR[0] &= ~PORT_PCR_MUX_MASK;
    PORTB->PCR[0] |=  PORT_PCR_MUX(0);

    adc_calibrate();

    ADC0->CFG1 &= ~ADC_CFG1_ADIV_MASK;
    ADC0->CFG1 |=  ADC_CFG1_ADIV(1);
    ADC0->CFG1 |=  ADC_CFG1_ADLSMP_MASK;
    ADC0->CFG1 &= ~ADC_CFG1_MODE_MASK;
    ADC0->CFG1 |=  ADC_CFG1_MODE(0b11);
    ADC0->CFG1 &= ~ADC_CFG1_ADICLK_MASK;
    ADC0->CFG1 |=  ADC_CFG1_ADICLK(0b00);

    ADC0->SC2 &= ~ADC_SC2_ADTRG_MASK;
    ADC0->SC2 &= ~ADC_SC2_REFSEL_MASK;
    ADC0->SC2 |=  ADC_SC2_REFSEL(0b01);

    ADC0->SC3 |= ADC_SC3_AVGE_MASK;
    ADC0->SC3 |= ADC_SC3_AVGS(0b11);
    ADC0->SC3 &= ~ADC_SC3_ADCO_MASK;
    ADC0->SC3 |=  ADC_SC3_ADCO(0);

    ADC0->SC1[0] &= ~ADC_SC1_ADCH_MASK;
    ADC0->SC1[0] |=  ADC_SC1_ADCH(0x1FU);
}

uint16_t sound_sensor_read(void){
    /* Single atomic write — no interrupt for polling */
    ADC0->SC1[0] = ADC_SC1_ADCH(SOUND_ADC_CHANNEL);

    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0U) {}

    return (uint16_t)(ADC0->R[0] & 0xFFFFU);
}

void vSoundTask(void *pvParameters){
    (void)pvParameters;

    uint32_t sum = 0U;
    uint32_t count = 0U;
    uint16_t sample = 0U;
    uint16_t baseline = 0U;

    static uint16_t triggerCount30s = 0U;
    static bool prevTriggered = false;
    static TickType_t windowStart = 0;
    static TickType_t lastEventTick = 0;

    const TickType_t windowTicks = pdMS_TO_TICKS(30000);
    const TickType_t rearmTicks  = pdMS_TO_TICKS(300);   // debounce / cooldown

    // ---- Phase 1: Calibration ----
    for (uint32_t t = 0U; t < CAL_TIME_MS; t += SAMPLE_DELAY_MS)
    {
        xSemaphoreTake(gADCMutex, portMAX_DELAY);
        sound_sensor_init();
        sample = sound_sensor_read();
        xSemaphoreGive(gADCMutex);

        sum += sample;
        count++;
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
    }

    baseline = (uint16_t)(sum / count);

    windowStart = xTaskGetTickCount();

    while (1)
    {
        uint16_t peak = 0, trough = 0xFFFF, aboveCount = 0;

        xSemaphoreTake(gADCMutex, portMAX_DELAY);
        for (uint32_t i = 0U; i < PEAK_SAMPLE_COUNT; i++)
        {
            sample = sound_sensor_read();

            if (sample > peak) peak = sample;
            if (sample < trough) trough = sample;

            uint16_t dev = (sample > baseline) ? (sample - baseline) : (baseline - sample);
            if (dev > TRIGGER_DELTA) aboveCount++;
        }
        xSemaphoreGive(gADCMutex);

        uint16_t swing = (peak > trough) ? (peak - trough) : 0U;
        bool issustained = (aboveCount >= PEAK_SAMPLE_COUNT / 10);
        bool isloud = (swing > TRIGGER_DELTA);
        bool triggered = isloud && issustained;

        TickType_t now = xTaskGetTickCount();

        // Count only a new trigger event
        if (triggered && !prevTriggered && (now - lastEventTick >= rearmTicks))
        {
            triggerCount30s++;
            lastEventTick = now;
        }
        prevTriggered = triggered;

        // Reset every 30 seconds
        if ((now - windowStart) >= windowTicks)
        {
            triggerCount30s = 0U;
            windowStart = now;
        }

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            gSensorData.sound_raw = peak;
            gSensorData.sound_triggered = triggered ? 1U : 0U;
            soundTriggerCount30s = triggerCount30s;
            xSemaphoreGive(gSensorMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
    }
}
