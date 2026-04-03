#ifndef ENV_CONDITION_H_
#define ENV_CONDITION_H_

#define LIGHT_TOO_DARK_THRESHOLD 1000U /* ADC raw above this = too dark */
#define SOUND_TOO_LOUD_THRESHOLD 2000U /* ADC raw above this = too loud */
#define TEMP_TOO_HOT_CELSIUS 33 /* degrees C */
#define TEMP_TOO_COLD_CELSIUS 18

void vEnvTask(void *pvParameters);

#endif /* ENV_CONDITION_H_ */
