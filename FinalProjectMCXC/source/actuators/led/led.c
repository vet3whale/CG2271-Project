#include "led.h"
#include "../../shared_data/shared_data.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "../../uart/uart_packet.h"

void led_init(void) {
	// enable clocks for LED ports
	SIM->SCGC5 |= (RED_PORT_CLK | GREEN_PORT_CLK);
	SIM->SCGC6 |= (RED_TPM_CLK | GREEN_TPM_CLK);

	SIM->SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK;
	SIM->SOPT2 |= SIM_SOPT2_TPMSRC(1);

	RED_PORT->PCR[RED_PIN] &= ~PORT_PCR_MUX_MASK;
	RED_PORT->PCR[RED_PIN] |= PORT_PCR_MUX(RED_MUX);

	GREEN_PORT->PCR[GREEN_PIN] &= ~PORT_PCR_MUX_MASK;
	GREEN_PORT->PCR[GREEN_PIN] |= PORT_PCR_MUX(GREEN_MUX);

	// set TPM base config, edge-alligned
	RED_TPM->SC = 0U;
	RED_TPM->CNT = 0U;
	RED_TPM->MOD = LED_MOD;
	RED_TPM->SC = TPM_SC_PS(TPM_PS);
	GREEN_TPM->SC = 0U;
	GREEN_TPM->CNT = 0U;
	GREEN_TPM->MOD = LED_MOD;
	GREEN_TPM->SC = TPM_SC_PS(TPM_PS);

	RED_TPM->CONTROLS[RED_TPM_CHANNEL].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
	RED_TPM->CONTROLS[RED_TPM_CHANNEL].CnV = 0;

	GREEN_TPM->CONTROLS[GREEN_TPM_CHANNEL].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
	GREEN_TPM->CONTROLS[GREEN_TPM_CHANNEL].CnV = 0;

}

static uint32_t current_r = 0;
static uint32_t current_g = 0;

void vLEDTask(void *pvParameters) {
	(void) pvParameters;

	while (1) {
		if (led_state) {
			uint32_t target_r = (g_color_blend * LED_CV) / 100;
			uint32_t target_g = ((100 - g_color_blend) * LED_CV) / 100;

			const uint32_t step = 10;

			if (current_r < target_r)
				current_r = (current_r + step > target_r) ? target_r : current_r + step;
			else if (current_r > target_r)
				current_r = (current_r < step) ? 0 : current_r - step;

			if (current_g < target_g)
				current_g = (current_g + step > target_g) ? target_g : current_g + step;
			else if (current_g > target_g)
				current_g = (current_g < step) ? 0 : current_g - step;

			RED_TPM->CONTROLS[RED_TPM_CHANNEL].CnV = current_r;
			GREEN_TPM->CONTROLS[GREEN_TPM_CHANNEL].CnV = current_g;

			RED_TPM->SC |= TPM_SC_CMOD(1);
			GREEN_TPM->SC |= TPM_SC_CMOD(1);
		} else {
			current_r = 0;
			current_g = 0;
			// Update duty cycles (0 = OFF, CV = ON)
			RED_TPM->CONTROLS[RED_TPM_CHANNEL].CnV = 0;
			GREEN_TPM->CONTROLS[GREEN_TPM_CHANNEL].CnV = 0;

			// Ensure all required TPMs are running
			RED_TPM->SC |= TPM_SC_CMOD(1);
			GREEN_TPM->SC |= TPM_SC_CMOD(1);
		}

		vTaskDelay(pdMS_TO_TICKS(20));
	}
}
