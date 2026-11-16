#pragma once

#include "devices.h"

inline uint8_t priorityOf(ControlSource source) { return static_cast<uint8_t>(source); }

inline const char* sourceName(ControlSource source) {
  switch (source) {
    case ControlSource::Technician: return "technician";
    case ControlSource::Teacher: return "teacher";
    case ControlSource::Physical: return "physical";
    case ControlSource::Automation: return "automation";
  }
  return "unknown";
}

inline bool mayReplace(ControlSource incoming, ControlSource current) {
  // Human commands are last-command-wins. Automation yields to a channel
  // that has been explicitly controlled by a person.
  if (incoming == ControlSource::Automation) return current == ControlSource::Automation;
  return true;
}
