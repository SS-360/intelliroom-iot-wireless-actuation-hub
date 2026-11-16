#pragma once

#include <Arduino.h>

inline void sendRelayMask(HardwareSerial& serial, uint16_t mask) {
  char command[8];
  snprintf(command, sizeof(command), "M,%04X\n", mask);
  serial.print(command);
}
