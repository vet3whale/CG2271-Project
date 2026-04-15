#ifndef ENV_CONDITION_H_
#define ENV_CONDITION_H_

#define LIGHT_TOO_DARK_THRESHOLD 1200U
#define SOUND_TOO_LOUD_THRESHOLD 2200U
#define TEMP_TOO_HOT_CELSIUS 30 /* degrees C */
#define TEMP_TOO_COLD_CELSIUS 18
#define HUM_TOO_HIGH 75
#define NOISY_TRIGGER_COUNT_30S 8U


void vEnvTask(void *pvParameters);

#endif /* ENV_CONDITION_H_ */
