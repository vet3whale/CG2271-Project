#include "dht_sensor.h"
#include "shared_data.h"
#include "uart_packet.h"        /* shared defines: PACKET_START1, TEMP_PKT_*, etc. */
#include <DHT.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static DHT   sDHT(DHT_PIN, DHT11);
static float sLastTemp     = 0.0f;
static float sLastHumidity = 0.0f;
static float sPrevTemp     = 0.0f;
static float sPrevHumidity = 0.0f;

static bool isValidReading(float temp, float humidity)
{
    return !(isnan(temp) || isnan(humidity));
}

static void printReading(float temp, float humidity)
{
    float tempChange = temp - sPrevTemp;
    float humChange  = humidity - sPrevHumidity;

    Serial.println("─────────────────────────────");
    Serial.print("Temp: ");          Serial.print(temp);     Serial.println(" C");
    Serial.print("Humidity: ");      Serial.print(humidity); Serial.println(" %");
    Serial.print("Temp change: ");
    Serial.print(tempChange > 0 ? "+" : "");
    Serial.print(tempChange);        Serial.println(" C");
    Serial.print("Humidity change: ");
    Serial.print(humChange  > 0 ? "+" : "");
    Serial.print(humChange);         Serial.println(" %");
}

/*
 * Builds and sends a 6-byte temperature packet to MCXC over UART.
 *
 * Packet layout (from uart_packet.h):
 * [0] PACKET_START1  (0xAA)
 * [1] TEMP_PKT_START2 (0x56)  ← disambiguates from LED packet (0x55)
 * [2] temp_int   — signed integer part  (e.g. 26 for 26.7°C)
 * [3] temp_frac  — first decimal digit  (e.g.  7 for 26.7°C)
 * [4] checksum   — XOR of bytes [2] and [3]
 * [5] PACKET_END (0xBB)
 */
static void sendTempPacket(float temperature)
{
    int8_t  ti = (int8_t)temperature;
    uint8_t tf = (uint8_t)((temperature - (float)ti) * 10.0f);

    uint8_t pkt[TEMP_PKT_LEN];
    pkt[0] = PACKET_START1;
    pkt[1] = TEMP_PKT_START2;
    pkt[2] = (uint8_t)ti;
    pkt[3] = tf;
    pkt[4] = TEMP_PKT_CHECKSUM(ti, tf);
    pkt[5] = PACKET_END;

    Serial2.write(pkt, TEMP_PKT_LEN);

    /* Debug */
    Serial.print("[DHT → MCXC] Sent temp packet: ");
    Serial.print(ti); Serial.print("."); Serial.println(tf);
}

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

void vDHTTask(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        float humidity    = sDHT.readHumidity();
        float temperature = sDHT.readTemperature();

        /* Skip bad reads — retry after 2s to let sensor recover */
        if (!isValidReading(temperature, humidity)) {
            Serial.println("Bad reading — retrying...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        printReading(temperature, humidity);

        /* Update change tracking for next print */
        sPrevTemp     = sLastTemp;
        sPrevHumidity = sLastHumidity;
        sLastTemp     = temperature;
        sLastHumidity = humidity;

        /* Write to shared data so other ESP32 tasks can read it */
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            gSensorData.esp_temp     = temperature;
            gSensorData.esp_humidity = humidity;
            /* NOTE: led_status no longer set here — env_condition
               is now calculated on MCXC and returned in TX packet */
            xSemaphoreGive(gSensorMutex);
        }

        /* Send temperature to MCXC — triggers vEnvTask on the other side */
        sendTempPacket(temperature);

        vTaskDelay(pdMS_TO_TICKS(DHT_POLL_MS));   /* 1000ms */
    }
}