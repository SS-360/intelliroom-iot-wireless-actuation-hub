#pragma once

#include <Arduino.h>
#include <PZEM004Tv30.h>

struct EnergyReading {
  float voltage = NAN;
  float current = NAN;
  float power = NAN;
  float energy = NAN;
  float frequency = NAN;
  float powerFactor = NAN;
  bool valid = false;
  uint32_t timestamp = 0;
};

inline EnergyReading energyReading;

inline bool readEnergy(PZEM004Tv30& pzem) {
  EnergyReading next;
  next.voltage = pzem.voltage();
  next.current = pzem.current();
  next.power = pzem.power();
  next.energy = pzem.energy();
  next.frequency = pzem.frequency();
  next.powerFactor = pzem.pf();
  next.timestamp = millis();
  next.valid = !isnan(next.voltage) && !isnan(next.current) && !isnan(next.power) &&
               !isnan(next.energy) && !isnan(next.frequency) && !isnan(next.powerFactor);
  energyReading = next;
  return next.valid;
}
