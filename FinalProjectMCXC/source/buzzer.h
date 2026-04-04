#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include "MCXC444.h"
#include "FreeRTOS.h"
#include "task.h"

/* --- Buzzer Hardware Mapping --- */
#define BUZZ_PORT            PORTA
#define BUZZ_GPIO            PTA    // GPIO Base
#define BUZZ_PIN             1U
#define BUZZ_PORT_CLK        SIM_SCGC5_PORTA_MASK
#define BUZZ_MUX             1U     // ALT1 = GPIO

/* --- Timing Settings --- */
#define BUZZ_ENV_COOLDOWN_MS 60000U

typedef enum {
    BUZZ_NONE = 0,
    BUZZ_SHORT,
    BUZZ_LONG
} buzz_req_t;

extern volatile buzz_req_t gBuzzerRequest;

void buzzer_init(void);
void vBuzzerTask(void *pvParameters);

#endif
