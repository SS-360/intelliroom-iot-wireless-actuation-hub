#pragma once

#include <Arduino.h>

constexpr uint8_t RELAY_COUNT = 8;
constexpr bool RELAY_ACTIVE_LOW = true;

// Channel 1 is bit 0, channel 8 is bit 7.
const uint8_t RELAY_PINS[RELAY_COUNT] = {
  2, 3, 4, 5, 6, 7, 8, 9
};

inline void writeRelay(uint8_t channel, bool on) {
  if (channel >= RELAY_COUNT) return;
  const uint8_t level = RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW);
  digitalWrite(RELAY_PINS[channel], level);
}

inline void applyRelayMask(uint16_t mask) {
  for (uint8_t channel = 0; channel < RELAY_COUNT; ++channel) {
    writeRelay(channel, (mask & (uint16_t(1) << channel)) != 0);
  }
}
