#pragma once

#include <IPAddress.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <WiFi.h>
#include "config.h"
#include "devices.h"
#include "energy.h"

inline void printLcdLine(LiquidCrystal_I2C& lcd, uint8_t row, const char* text) {
  char padded[21];
  snprintf(padded, sizeof(padded), "%-20.20s", text);
  lcd.setCursor(0, row);
  lcd.print(padded);
}

inline void drawLcd(LiquidCrystal_I2C& lcd, bool remoteMode, bool wifiOnline,
                    const IPAddress& ipAddress, bool allOffLock, uint8_t page, time_t now,
                    const uint8_t* buttonPins, const uint16_t* buttonMasks) {
  char line[21];
  struct tm localTime;
  const bool timeReady = now > 1700000000 && localtime_r(&now, &localTime);
  char clockText[9] = "--:--:--";
  if (timeReady) snprintf(clockText, sizeof(clockText), "%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);

  switch (page % 6) {
    case 0:
      printLcdLine(lcd, 0, "IntelliRoom / LIVE");
      snprintf(line, sizeof(line), "%s  %s", remoteMode ? "Remote mode" : "Manual mode", clockText); printLcdLine(lcd, 1, line);
      snprintf(line, sizeof(line), "Relays active %u/8", commandedOnCount()); printLcdLine(lcd, 2, line);
      if (energyReading.valid) snprintf(line, sizeof(line), "Load now %.0f W", energyReading.power); else snprintf(line, sizeof(line), "Power meter offline");
      printLcdLine(lcd, 3, line); break;
    case 1: {
      printLcdLine(lcd, 0, "Relay command state");
      char first[5] = {}, second[5] = {};
      for (uint8_t i = 0; i < 4; ++i) { first[i] = devices[i].desiredState ? '1' : '0'; second[i] = devices[i + 4].desiredState ? '1' : '0'; }
      snprintf(line, sizeof(line), "CH 1-4: %s", first); printLcdLine(lcd, 1, line);
      snprintf(line, sizeof(line), "CH 5-8: %s", second); printLcdLine(lcd, 2, line);
      snprintf(line, sizeof(line), "Active %u  %s", commandedOnCount(), allOffLock ? "All-off lock" : clockText); printLcdLine(lcd, 3, line); break;
    }
    case 2:
      printLcdLine(lcd, 0, "Electrical readings");
      if (energyReading.valid) {
        snprintf(line, sizeof(line), "Voltage %.1f V", energyReading.voltage); printLcdLine(lcd, 1, line);
        snprintf(line, sizeof(line), "Current %.2f A", energyReading.current); printLcdLine(lcd, 2, line);
        snprintf(line, sizeof(line), "Power %.0f W", energyReading.power); printLcdLine(lcd, 3, line);
      } else { printLcdLine(lcd, 1, "PZEM reading missing"); printLcdLine(lcd, 2, "Check meter wiring"); printLcdLine(lcd, 3, clockText); }
      break;
    case 3:
      printLcdLine(lcd, 0, "Energy meter");
      if (energyReading.valid) {
        snprintf(line, sizeof(line), "Energy %.2f kWh", energyReading.energy); printLcdLine(lcd, 1, line);
        snprintf(line, sizeof(line), "Frequency %.1f Hz", energyReading.frequency); printLcdLine(lcd, 2, line);
        snprintf(line, sizeof(line), "Power factor %.2f", energyReading.powerFactor); printLcdLine(lcd, 3, line);
      } else { printLcdLine(lcd, 1, "Waiting for PZEM"); printLcdLine(lcd, 2, "No meter data"); printLcdLine(lcd, 3, clockText); }
      break;
    case 4:
      printLcdLine(lcd, 0, "Network / controller");
      if (wifiOnline) snprintf(line, sizeof(line), "IP %u.%u.%u.%u", ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]); else snprintf(line, sizeof(line), "Wi-Fi connecting");
      printLcdLine(lcd, 1, line);
      if (wifiOnline) snprintf(line, sizeof(line), "Wi-Fi signal %d dBm", WiFi.RSSI()); else snprintf(line, sizeof(line), "Check access point");
      printLcdLine(lcd, 2, line);
      snprintf(line, sizeof(line), "%s  %s", timeReady ? "Clock synced" : "Clock not synced", clockText); printLcdLine(lcd, 3, line);
      break;
    default: {
      printLcdLine(lcd, 0, "H=idle; L=pressed");
      snprintf(line, sizeof(line), "%u:%c %u:%c %u:%c", buttonPins[0], digitalRead(buttonPins[0]) == LOW ? 'L' : 'H', buttonPins[1], digitalRead(buttonPins[1]) == LOW ? 'L' : 'H', buttonPins[2], digitalRead(buttonPins[2]) == LOW ? 'L' : 'H');
      printLcdLine(lcd, 1, line);
      snprintf(line, sizeof(line), "%u:%c %u:%c mode", buttonPins[3], digitalRead(buttonPins[3]) == LOW ? 'L' : 'H', MODE_TOGGLE_PIN, digitalRead(MODE_TOGGLE_PIN) == LOW ? 'L' : 'H');
      printLcdLine(lcd, 2, line);
      snprintf(line, sizeof(line), "1-4: %02X %02X %02X %02X", buttonMasks[0] & 0xFF, buttonMasks[1] & 0xFF, buttonMasks[2] & 0xFF, buttonMasks[3] & 0xFF);
      printLcdLine(lcd, 3, line);
      break;
    }
  }
}
