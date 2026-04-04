#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "MCXC444.h"
#include "FreeRTOS.h"
#include "task.h"

#define RED_CV              LED_CV
#define GREEN_CV            LED_CV
#define BLUE_CV             LED_CV

/* --- RED LED (PTE21) --- */
#define RED_PORT            PORTE
#define RED_PIN             29U
#define RED_PORT_CLK        SIM_SCGC5_PORTE_MASK
#define RED_TPM             TPM0
#define RED_TPM_CLK         SIM_SCGC6_TPM0_MASK
#define RED_TPM_CHANNEL     2U
#define RED_MUX             3U    // ALT3 = TPM1_CH1

/* --- GREEN LED (PTE20) --- */
#define GREEN_PORT          PORTE
#define GREEN_PIN           30U
#define GREEN_PORT_CLK      SIM_SCGC5_PORTE_MASK
#define GREEN_TPM           TPM0
#define GREEN_TPM_CLK       SIM_SCGC6_TPM0_MASK
#define GREEN_TPM_CHANNEL   3U
#define GREEN_MUX           3U    // ALT3 = TPM1_CH0

/* --- BLUE LED (PTE22) --- */
#define BLUE_PORT           PORTE
#define BLUE_PIN            31U // intentionally cut off, kept if i want to use later
#define BLUE_PORT_CLK       SIM_SCGC5_PORTE_MASK
#define BLUE_TPM            TPM2
#define BLUE_TPM_CLK        SIM_SCGC6_TPM2_MASK
#define BLUE_TPM_CHANNEL    0U
#define BLUE_MUX            3U    // ALT3 = TPM2_CH0

/* --- PWM / Timing Settings --- */
#define LED_PWM_FREQ_HZ     1000U
#define LED_DUTY_PCT        70U
#define LED_POLL_MS         50U
#define LED_TASK_STACK      128U

#define TPM_CLK_HZ          48000000U
#define TPM_PS              3U
#define TPM_FCLK            (TPM_CLK_HZ / 8U)
#define LED_MOD             ((TPM_FCLK / LED_PWM_FREQ_HZ) - 1U)
#define LED_CV              (LED_MOD * LED_DUTY_PCT / 100U)

void led_init(void);
void vLEDTask(void *pvParameters);

#endif /* LED_H */
