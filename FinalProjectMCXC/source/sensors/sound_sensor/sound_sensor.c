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

/*
 * One-time HW init: clocks, pin mux, ADC self-calibration (RM §23.5.6).
 * Call ONCE at startup while holding gADCMutex.
 * Calibration requirements per RM: ADTRG=0, fADCK ≤ 4 MHz, 32x HW averaging.
 */
void sound_sensor_hw_init(void)
{
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[0] = 0U;                   /* PTB0 → ALT0 (analog), no pull */

    NVIC_DisableIRQ(ADC0_IRQn);

    /* Calibration config: bus/8, long sample, 16-bit, VDDA ref, 32x avg */
    ADC0->CFG1 = ADC_CFG1_ADIV(3) | ADC_CFG1_ADLSMP_MASK | ADC_CFG1_MODE(3);
    ADC0->SC2  = ADC_SC2_REFSEL(1);
    ADC0->SC3  = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3) | ADC_SC3_CAL_MASK;

    /* Wait for calibration to complete (~1.7 ms at 8 MHz per RM §23.5.6) */
    while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK)) {}

    if (ADC0->SC3 & ADC_SC3_CALF_MASK)
    {
        ADC0->SC3 |= ADC_SC3_CALF_MASK;   /* clear CALF flag, skip gain write */
    }
    else
    {
        uint16_t cal;

        /* Plus-side gain: sum CLP registers, halve, set MSB */
        cal = (uint16_t)(ADC0->CLPD + ADC0->CLPS + ADC0->CLP4 +
                         ADC0->CLP3 + ADC0->CLP2 + ADC0->CLP1 + ADC0->CLP0);
        ADC0->PG = (uint32_t)((cal >> 1U) | 0x8000U);

        /* Minus-side gain */
        cal = (uint16_t)(ADC0->CLMD + ADC0->CLMS + ADC0->CLM4 +
                         ADC0->CLM3 + ADC0->CLM2 + ADC0->CLM1 + ADC0->CLM0);
        ADC0->MG = (uint32_t)((cal >> 1U) | 0x8000U);
    }

    NVIC_EnableIRQ(ADC0_IRQn);            /* restore for light-sensor ISR */
}

/*
 * Reconfigure ADC0 for sound polling (call while holding gADCMutex).
 * Disables ADC0_IRQn to prevent stale COCO from firing light ISR during
 * the polling burst. Caller MUST call NVIC_EnableIRQ(ADC0_IRQn) after
 * all reads are complete, before releasing gADCMutex.
 */
void sound_sensor_init(void)
{
    NVIC_DisableIRQ(ADC0_IRQn);
    NVIC_ClearPendingIRQ(ADC0_IRQn);
    ADC0->SC1[0] = ADC_SC1_ADCH(0x1FU);  /* abort any in-flight conversion */
    ADC0->CFG1   = ADC_CFG1_ADIV(1) | ADC_CFG1_ADLSMP_MASK | ADC_CFG1_MODE(3);
    ADC0->SC2    = ADC_SC2_REFSEL(1);
    ADC0->SC3    = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3);  /* 32x avg, single shot */
    /* NVIC_EnableIRQ intentionally omitted — caller restores after reads */
}

/* Single polled read; AIEN is never set so no interrupt fires */
uint16_t sound_sensor_read(void)
{
    ADC0->SC1[0] = ADC_SC1_ADCH(SOUND_ADC_CHANNEL);
    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0U) {}
    return (uint16_t)ADC0->R[0];          /* cast alone truncates to 16 bits */
}

void vSoundTask(void *pvParameters)
{
    (void)pvParameters;

    uint32_t sum   = 0U;
    uint32_t count = 0U;
    uint16_t sample   = 0U;
    uint16_t baseline = 0U;

    static uint16_t    triggerCount30s = 0U;
    static bool        prevTriggered   = false;
    static TickType_t  windowStart     = 0;
    static TickType_t  lastEventTick   = 0;

    const TickType_t windowTicks = pdMS_TO_TICKS(30000);
    const TickType_t rearmTicks  = pdMS_TO_TICKS(300);    /* debounce cooldown */

    /* ---- Phase 1: One-time HW init + calibration ---- */
    xSemaphoreTake(gADCMutex, portMAX_DELAY);
    sound_sensor_hw_init();
    xSemaphoreGive(gADCMutex);

    /* ---- Phase 2: Baseline sampling ---- */
    for (uint32_t t = 0U; t < CAL_TIME_MS; t += SAMPLE_DELAY_MS)
    {
        xSemaphoreTake(gADCMutex, portMAX_DELAY);
        sound_sensor_init();               /* ADC reconfigure only, no calibration */
        sample = sound_sensor_read();
        NVIC_EnableIRQ(ADC0_IRQn);        /* restore for light ISR before release */
        xSemaphoreGive(gADCMutex);

        sum += sample;
        count++;
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
    }

    baseline    = (uint16_t)(sum / count);
    windowStart = xTaskGetTickCount();

    /* ---- Phase 3: Main detection loop ---- */
    while (1)
    {
        uint16_t peak = 0U, trough = 0xFFFFU, aboveCount = 0U;

        xSemaphoreTake(gADCMutex, portMAX_DELAY);
        sound_sensor_init();               /* reconfigure — light task may have run */
        for (uint32_t i = 0U; i < PEAK_SAMPLE_COUNT; i++)
        {
            sample = sound_sensor_read();

            if (sample > peak)   peak   = sample;
            if (sample < trough) trough = sample;

            uint16_t dev = (sample > baseline) ? (sample - baseline) : 0;
            if (dev > TRIGGER_DELTA) aboveCount++;
        }
        NVIC_EnableIRQ(ADC0_IRQn);        /* restore for light ISR before release */
        xSemaphoreGive(gADCMutex);

        uint16_t swing      = (peak > trough) ? (peak - trough) : 0U;
        bool     issustained = (aboveCount >= PEAK_SAMPLE_COUNT / 10U);
        bool     isloud      = (swing > TRIGGER_DELTA);
        bool     triggered   = isloud && issustained;

        TickType_t now = xTaskGetTickCount();

        /* Count only a fresh trigger edge, with debounce cooldown */
        if (triggered && (now - lastEventTick >= rearmTicks))
        {
            triggerCount30s++;
            lastEventTick = now;
        }
        prevTriggered = triggered;


        /* Reset window counter every 30 s */
        if ((now - windowStart) >= windowTicks)
        {
            triggerCount30s = 0U;
            windowStart     = now;
        }

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            gSensorData.sound           = peak;
            gSensorData.sound_triggered = triggered ? 1U : 0U;
            soundTriggerCount30s        = triggerCount30s;
            xSemaphoreGive(gSensorMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
    }
}
