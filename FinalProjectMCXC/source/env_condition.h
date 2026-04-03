#ifndef ENV_CONDITION_H_
#define ENV_CONDITION_H_

#define LIGHT_TOO_DARK_THRESHOLD 500U /* ADC raw above this = too dark */
#define SOUND_TOO_LOUD_THRESHOLD 1800U /* ADC raw above this = too loud */
#define TEMP_TOO_HOT_CELSIUS 30 /* degrees C */
#define TEMP_TOO_COLD_CELSIUS 18

void vEnvTask(void *pvParameters);

#endif /* ENV_CONDITION_H_ */
