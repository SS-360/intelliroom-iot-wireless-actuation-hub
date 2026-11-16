#pragma once

#include <Arduino.h>

enum class ControlSource : uint8_t {
  Physical = 1,
  Technician = 2,
  Teacher = 3,
  Automation = 4
};

struct DeviceState {
  uint8_t channel; // 1-8; its relay bit is channel - 1
  const char* id;
  char name[48];
  bool desiredState;
  ControlSource source;
  uint32_t lastChangedAt;
};

constexpr uint8_t DEVICE_COUNT = 8;
inline DeviceState devices[DEVICE_COUNT] = {
  {1, "light_1", "Channel 1", false, ControlSource::Automation, 0},
  {2, "light_2", "Channel 2", false, ControlSource::Automation, 0},
  {3, "light_3", "Channel 3", false, ControlSource::Automation, 0},
  {4, "light_4", "Channel 4", false, ControlSource::Automation, 0},
  {5, "fan_1", "Channel 5", false, ControlSource::Automation, 0},
  {6, "fan_2", "Channel 6", false, ControlSource::Automation, 0},
  {7, "fan_3", "Channel 7", false, ControlSource::Automation, 0},
  {8, "fan_4", "Channel 8", false, ControlSource::Automation, 0},
};

inline DeviceState* findDevice(const char* id) {
  for (auto& device : devices) if (strcmp(device.id, id) == 0) return &device;
  return nullptr;
}

inline uint16_t commandedRelayMask() {
  uint16_t mask = 0;
  for (const auto& device : devices) if (device.desiredState) mask |= uint16_t(1) << (device.channel - 1);
  return mask;
}

inline uint8_t commandedOnCount() {
  uint8_t count = 0;
  for (const auto& device : devices) if (device.desiredState) ++count;
  return count;
}
