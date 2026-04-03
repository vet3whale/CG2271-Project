#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H
#include <Arduino.h>

#define DHT_PIN             20
#define DHT_TASK_PRIORITY   1
#define DHT_TASK_STACK_SIZE 2048
#define DHT_POLL_MS         1000    /* was 2000 — now matches 1s MCXC expectation */

void  DHT_Init(void);
void  vDHTTask(void *pvParameters);
float DHT_GetTemp(void);
float DHT_GetHumidity(void);

#endif // DHT_SENSOR_H