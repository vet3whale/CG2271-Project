#include "dht_sensor.h"
#include "shared_data.h"
#include "uart_tx.h"            /* UART_TX_SendTemp() */
#include <DHT.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ── Internal state ──────────────────────────────────────────────────────── */
static DHT   sDHT(DHT_PIN, DHT11);
static float sLastTemp     = 0.0f;
static float sLastHumidity = 0.0f;
static float sPrevTemp     = 0.0f;
static float sPrevHumidity = 0.0f;

/* ── Private helpers ─────────────────────────────────────────────────────── */
static bool isValidReading(float temp, float humidity)
{
    if (isnan(temp) || isnan(humidity)) return false;
    return true;
}

static void printReading(float temp, float humidity)
{
    float tempChange = temp - sPrevTemp;
    float humChange  = humidity - sPrevHumidity;

    Serial.println("─────────────────────────────");
    Serial.print("Temp: ");     Serial.print(temp);     Serial.println(" C");
    Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
    Serial.println("─────────────────────────────");
}

/* ═════════════════════════════════════════════════════════════════════════ */
/* PUBLIC FUNCTIONS                                                          */
/* ═════════════════════════════════════════════════════════════════════════ */
void DHT_Init(void)
{
    sDHT.begin();
    Serial.println("Waiting for sensor to stabilise...");
    delay(2000);
    Serial.println("Ready!");
    Serial.println("─────────────────────────────");
}

float DHT_GetTemp(void)     { return sLastTemp; }
float DHT_GetHumidity(void) { return sLastHumidity; }

/* ═════════════════════════════════════════════════════════════════════════ */
/* TASK                                                                      */
/* ═════════════════════════════════════════════════════════════════════════ */
void vDHTTask(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        float humidity    = sDHT.readHumidity();
        float temperature = sDHT.readTemperature();

        /* Skip bad reads — give sensor 2s to recover */
        if (!isValidReading(temperature, humidity)) {
            Serial.println("Bad reading — retrying...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        // printReading(temperature, humidity);

        /* Update change tracking */
        sPrevTemp     = sLastTemp;
        sPrevHumidity = sLastHumidity;
        sLastTemp     = temperature;
        sLastHumidity = humidity;

        /* Write to shared data for other ESP32 tasks (Telegram, monitor, etc.) */
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            gSensorData.esp_temp     = temperature;
            gSensorData.esp_humidity = humidity;

            xSemaphoreGive(gSensorMutex);
        }

        /* Send temp to MCXC — this triggers vRXTask → gTempReadySem → vEnvTask */
        int8_t  ti = (int8_t)temperature;
        uint8_t tf = (uint8_t)((temperature - (float)ti) * 10.0f);
        UART_TX_SendTemp(ti, tf);

        vTaskDelay(pdMS_TO_TICKS(DHT_POLL_MS));
    }
}