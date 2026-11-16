#pragma once

#include <Arduino.h>

// Do not commit production credentials. These prototype values must be changed.
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char TECHNICIAN_USERNAME[] = "technician";
constexpr char TECHNICIAN_PASSWORD[] = "change-me-technician";
constexpr char TEACHER_USERNAME[] = "teacher";
constexpr char TEACHER_PASSWORD[] = "change-me-teacher";

constexpr uint16_t HTTP_PORT = 80;
constexpr uint32_t UNO_BAUD = 9600;
constexpr uint32_t PZEM_BAUD = 9600;
constexpr uint32_t ENERGY_POLL_INTERVAL_MS = 2000;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 45;
constexpr uint32_t SESSION_TTL_MS = 8UL * 60UL * 60UL * 1000UL;
constexpr long TIMEZONE_OFFSET_SECONDS = 19800; // India Standard Time (UTC+05:30)

constexpr int LCD_SDA_PIN = 21;
constexpr int LCD_SCL_PIN = 22;
constexpr int PZEM_RX_PIN = 16; // ESP32 RX <- PZEM TX
constexpr int PZEM_TX_PIN = 14; // ESP32 TX -> PZEM RX
constexpr int UNO_TX_PIN = 17;  // ESP32 TX -> Uno D0/RX
constexpr int LIGHTS_BUTTON_PIN = 32;
constexpr int FANS_BUTTON_PIN = 33;
constexpr int ALL_ON_BUTTON_PIN = 25;
constexpr int ALL_OFF_BUTTON_PIN = 26;
constexpr int MODE_TOGGLE_PIN = 27;
