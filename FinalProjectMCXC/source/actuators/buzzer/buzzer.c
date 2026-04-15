#include "buzzer.h"

volatile buzz_req_t gBuzzerRequest = BUZZ_NONE;

void buzzer_init(void) {
	// 1. Enable Port Clock
	SIM->SCGC5 |= BUZZ_PORT_CLK;

	// 2. Set Pin Mux to GPIO (ALT1)
	BUZZ_PORT->PCR[BUZZ_PIN] &= ~PORT_PCR_MUX_MASK;
	BUZZ_PORT->PCR[BUZZ_PIN] |= PORT_PCR_MUX(BUZZ_MUX);

	// 3. Set Pin Direction to Output
	BUZZ_GPIO->PDDR |= (1U << BUZZ_PIN);

	// 4. Ensure it starts OFF
	BUZZ_GPIO->PCOR = (1U << BUZZ_PIN);
}

void vBuzzerTask(void *pvParameters) {
	TickType_t lastEnvBuzzTick = 0;

	while (1) {
		buzz_req_t currentReq = gBuzzerRequest;
		TickType_t now = xTaskGetTickCount();
		uint32_t duration = 0;

		if (currentReq == BUZZ_SHORT) {
			duration = 150;
			gBuzzerRequest = BUZZ_NONE;
		} else if (currentReq == BUZZ_LONG) {
			if ((now - lastEnvBuzzTick) >= pdMS_TO_TICKS(BUZZ_ENV_COOLDOWN_MS)) {
				duration = 800;
				lastEnvBuzzTick = now;
			}
			gBuzzerRequest = BUZZ_NONE;
		}

		if (duration > 0) {
			// Turn Buzzer ON (High)
			BUZZ_GPIO->PSOR = (1U << BUZZ_PIN);

			vTaskDelay(pdMS_TO_TICKS(duration));

			// Turn Buzzer OFF (Low)
			BUZZ_GPIO->PCOR = (1U << BUZZ_PIN);
		}

		vTaskDelay(pdMS_TO_TICKS(50));
	}
}
