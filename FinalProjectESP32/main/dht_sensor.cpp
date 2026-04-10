#include "dht_sensor.h"
#include "shared_data.h"
#include "uart_tx.h" /* UART_TX_SendTemp() */
#include <DHT.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static DHT sDHT(DHT_PIN, DHT11);

static bool isValidReading(float temp, float humidity) {
  if (isnan(temp) || isnan(humidity)) return false;
  return true;
}

void DHT_Init(void) {
  sDHT.begin();
  delay(500); // to stabilise
}

void vDHTTask(void *pvParameters) {
  (void)pvParameters;

  while (1) {
    float humidity = sDHT.readHumidity();
    float temperature = sDHT.readTemperature();

    if (!isValidReading(temperature, humidity)) continue;

    if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      gSensorData.esp_temp = temperature;
      gSensorData.esp_humidity = humidity;
      xSemaphoreGive(gSensorMutex);
    }

    // convert temp and humidity from 16 bit to 8 bit
    int8_t ti = (int8_t)temperature;
    uint8_t tf = (uint8_t)((temperature - (float)ti) * 10.0f);
    int8_t hi = (int8_t)humidity;
    uint8_t hf = (uint8_t)((humidity - (float)hi) * 10.0f);
    UART_TX_SendTemp(ti, tf, hi, hf);

    vTaskDelay(pdMS_TO_TICKS(DHT_POLL_MS));
  }
}