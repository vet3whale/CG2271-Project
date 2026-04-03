#include "led.h"
#include "shared_data.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "uart_packet.h"

/*
 * Helper function to set the RGB LED color
 * r, g, b: 1 to turn on the color, 0 to turn it off
 */
static void led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    // Update duty cycles (0 = OFF, CV = ON)
	RED_TPM->CONTROLS[RED_TPM_CHANNEL].CnV     = r ? LED_CV : 0U;
	GREEN_TPM->CONTROLS[GREEN_TPM_CHANNEL].CnV = g ? LED_CV : 0U;
	BLUE_TPM->CONTROLS[BLUE_TPM_CHANNEL].CnV   = b ? LED_CV : 0U;

    // Ensure all required TPMs are running
    RED_TPM->SC   |= TPM_SC_CMOD(1U);
    GREEN_TPM->SC |= TPM_SC_CMOD(1U);
    BLUE_TPM->SC  |= TPM_SC_CMOD(1U);
}

void led_init(void)
{
    /* Enable clocks for all LED ports and TPM modules */
    SIM->SCGC5 |= (RED_PORT_CLK | GREEN_PORT_CLK | BLUE_PORT_CLK);
    SIM->SCGC6 |= (RED_TPM_CLK | GREEN_TPM_CLK | BLUE_TPM_CLK);

    /* IRC48M as TPM clock source */
    SIM->SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK;
    SIM->SOPT2 |=  SIM_SOPT2_TPMSRC(1U);

    /* Configure Pin Muxing for all 3 LEDs */
    RED_PORT->PCR[RED_PIN]     &= ~PORT_PCR_MUX_MASK;
    RED_PORT->PCR[RED_PIN]     |=  PORT_PCR_MUX(RED_MUX);

    GREEN_PORT->PCR[GREEN_PIN] &= ~PORT_PCR_MUX_MASK;
    GREEN_PORT->PCR[GREEN_PIN] |=  PORT_PCR_MUX(GREEN_MUX);

    BLUE_PORT->PCR[BLUE_PIN]   &= ~PORT_PCR_MUX_MASK;
    BLUE_PORT->PCR[BLUE_PIN]   |=  PORT_PCR_MUX(BLUE_MUX);

    /* Setup TPM base configurations (edge-aligned PWM) */
    RED_TPM->SC = 0U;   RED_TPM->CNT = 0U;   RED_TPM->MOD = LED_MOD;   RED_TPM->SC = TPM_SC_PS(TPM_PS);
    GREEN_TPM->SC = 0U; GREEN_TPM->CNT = 0U; GREEN_TPM->MOD = LED_MOD; GREEN_TPM->SC = TPM_SC_PS(TPM_PS);
    BLUE_TPM->SC = 0U;  BLUE_TPM->CNT = 0U;  BLUE_TPM->MOD = LED_MOD;  BLUE_TPM->SC = TPM_SC_PS(TPM_PS);

    /* Setup TPM Channels (high-true EPWM) and start OFF */
    RED_TPM->CONTROLS[RED_TPM_CHANNEL].CnSC     = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
    RED_TPM->CONTROLS[RED_TPM_CHANNEL].CnV      = 0U;

    GREEN_TPM->CONTROLS[GREEN_TPM_CHANNEL].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
    GREEN_TPM->CONTROLS[GREEN_TPM_CHANNEL].CnV  = 0U;

    BLUE_TPM->CONTROLS[BLUE_TPM_CHANNEL].CnSC   = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
    BLUE_TPM->CONTROLS[BLUE_TPM_CHANNEL].CnV    = 0U;
}

void vLEDTask(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        // Check if Focus Mode / LED system is generally enabled
        if (led_state) {

            // Read environment condition.
            // 8-bit reads are atomic on ARM Cortex-M, so direct read is safe.
            uint8_t cond = gSensorData.env_condition;

            switch (cond) {
                case ENV_GOOD:
                    led_set_color(0, 1, 0); // Solid GREEN
                    break;

                case ENV_MODERATE:
                    led_set_color(1, 1, 0); // AMBER / YELLOW (Red + Green)
                    break;

                case ENV_POOR:
                    led_set_color(1, 0, 0); // Solid RED
                    break;

                default:
                    led_set_color(0, 0, 1); // Solid BLUE (fallback/idle)
                    break;
            }
        }
        else {
            // Turn all off if system is disabled/paused
            led_set_color(0, 0, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(LED_POLL_MS));
    }
}
