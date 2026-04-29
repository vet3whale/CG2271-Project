# CG2271-RTOS-Baymax

Baymax is a dual-microcontroller IoT system built on the NXP FRDM-MCXC444 and ESP32-S2, running on FreeRTOS. It acts as a study session timer that continuously monitors ambient light, sound, temperature, and humidity at a student's desk, providing helpful feedback to improve study productivity.

The MCXC444 runs FreeRTOS tasks handling sensor acquisition, EMA filtering, and actuation (RGB LED and buzzer), transmitting time-averaged sensor readings over UART to the ESP32-S2. The ESP32-S2 handles temperature and humidity sensing via a DHT11, drives an SSD1306 OLED display, and at the end of each session, delivers a personalised, constructive AI-generated study environment report through the Google Gemini API and real-time Telegram notifications.

## Report
[View Final Report](docs/CG2271_Baymax_Report.pdf)

## Team

**CG2271 Real-Time Operating Systems**, AY2025/26 Semester 2 Project done by:

| Name | GitHub |
|------|--------|
| Ariff Muhammed Ahsan Husain | [@Ariff1422](https://github.com/Ariff1422) |
| T. Kandasami | [@t-kandasami](https://github.com/t-kandasami) |
| Kevin Loke Wei Yi | [@kevinlokewy](https://github.com/kevinlokewy) |
| Karthikeyan Vetrivel | [@vet3whale](https://github.com/vet3whale) |
