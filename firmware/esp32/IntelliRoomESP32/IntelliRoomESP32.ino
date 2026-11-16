#include <WiFi.h>
#include <Wire.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <time.h>
#include <LiquidCrystal_I2C.h>
#include <PZEM004Tv30.h>

#include "config.h"
#include "devices.h"
#include "priority.h"
#include "protocol.h"
#include "energy.h"
#include "lcd_ui.h"
#include "api.h"
#include "dashboard_ui.h"

HardwareSerial relaySerial(1);
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4); // Change 0x27 if the LCD scan finds another address.
AsyncWebServer server(HTTP_PORT);
Preferences preferences;

constexpr uint8_t CUSTOM_GROUP_COUNT = 4;
struct CustomRelayGroup {
  char name[48] = "";
  uint16_t channelMask = 0;
};
CustomRelayGroup customRelayGroups[CUSTOM_GROUP_COUNT];
constexpr uint8_t SCHEDULE_COUNT = 4;
struct ScheduleRule {
  char name[32] = "";
  uint16_t channelMask = 0;
  uint16_t startMinute = 480;
  uint16_t endMinute = 1020;
  uint8_t weekdays = 0x3E; // Monday through Friday
  bool startOn = true;
  bool endOn = false;
  bool enabled = false;
};
ScheduleRule schedules[SCHEDULE_COUNT];
time_t timerDeadlines[DEVICE_COUNT] = {};
int32_t lastScheduleMinute[SCHEDULE_COUNT] = {-1, -1, -1, -1};

constexpr uint8_t PHYSICAL_BUTTON_COUNT = 4;
struct PhysicalButtonMapping {
  char name[48];
  uint16_t channelMask;
};
PhysicalButtonMapping physicalButtonMappings[PHYSICAL_BUTTON_COUNT] = {
  {"Button 1", 0x0F}, {"Button 2", 0xF0}, {"Button 3", 0x55}, {"Button 4", 0xAA}
};

struct ActivityLog {
  uint64_t timestampMs;
  const char* source;
  const char* target;
  const char* action;
};

constexpr uint8_t LOG_CAPACITY = 24;
ActivityLog activityLog[LOG_CAPACITY];
uint8_t logWriteIndex = 0;
uint8_t logCount = 0;

bool remoteMode = true;
bool physicalAllOffLock = false;
bool safetyInterlock = false;
bool pzemWasOnline = false;
bool wifiWasConnected = false;
uint32_t lastEnergyPoll = 0;
uint32_t lastLcdDraw = 0;
uint32_t lastWifiRetry = 0;
uint8_t lcdPage = 0;

struct DebouncedButton {
  uint8_t pin;
  bool lastRaw = HIGH;
  bool stable = HIGH;
  uint32_t lastChange = 0;
};

DebouncedButton physicalButtons[PHYSICAL_BUTTON_COUNT] = {
  {LIGHTS_BUTTON_PIN}, {FANS_BUTTON_PIN}, {ALL_ON_BUTTON_PIN}, {ALL_OFF_BUTTON_PIN}
};
const uint8_t lcdButtonPins[PHYSICAL_BUTTON_COUNT] = {
  LIGHTS_BUTTON_PIN, FANS_BUTTON_PIN, ALL_ON_BUTTON_PIN, ALL_OFF_BUTTON_PIN
};

void renderStatusLcd(uint8_t page) {
  uint16_t buttonMasks[PHYSICAL_BUTTON_COUNT];
  for (uint8_t index = 0; index < PHYSICAL_BUTTON_COUNT; ++index) buttonMasks[index] = physicalButtonMappings[index].channelMask;
  drawLcd(lcd, remoteMode, WiFi.status() == WL_CONNECTED, WiFi.localIP(), physicalAllOffLock,
          page, time(nullptr), lcdButtonPins, buttonMasks);
}

void addLog(const char* source, const char* target, const char* action) {
  const time_t now = time(nullptr);
  activityLog[logWriteIndex] = {now > 1700000000 ? uint64_t(now) * 1000ULL : 0, source, target, action};
  logWriteIndex = (logWriteIndex + 1) % LOG_CAPACITY;
  if (logCount < LOG_CAPACITY) ++logCount;
}

void transmitCommandedMask() {
  // The command is a command-state update, not an acknowledgement from the Uno.
  sendRelayMask(relaySerial, commandedRelayMask());
}

bool applyDeviceCommand(DeviceState& device, bool state, ControlSource incoming, bool force = false) {
  if (safetyInterlock && state) return false;
  if (incoming == ControlSource::Automation && (!remoteMode || physicalAllOffLock)) return false;
  if (!force && !mayReplace(incoming, device.source)) return false;

  device.desiredState = state;
  device.source = incoming;
  device.lastChangedAt = millis();
  transmitCommandedMask();
  addLog(sourceName(incoming), device.id, state ? "ON" : "OFF");
  return true;
}

void forceAllOff(ControlSource source, bool lockAutomation) {
  for (auto& device : devices) {
    device.desiredState = false;
    device.source = source;
    device.lastChangedAt = millis();
  }
  physicalAllOffLock = lockAutomation;
  transmitCommandedMask();
  addLog(sourceName(source), "all_devices", "ALL_OFF");
}

void commandGroup(uint8_t firstIndex, uint8_t lastIndex, ControlSource source) {
  bool anyOn = false;
  for (uint8_t i = firstIndex; i <= lastIndex; ++i) anyOn |= devices[i].desiredState;
  for (uint8_t i = firstIndex; i <= lastIndex; ++i) {
    applyDeviceCommand(devices[i], !anyOn, source);
  }
}

void togglePhysicalButton(uint8_t index) {
  if (remoteMode) return;
  if (index >= PHYSICAL_BUTTON_COUNT) return;
  const uint16_t mask = physicalButtonMappings[index].channelMask;
  if (mask == 0) {
    Serial.printf("Button %u has no relay channels assigned.\n", index + 1);
    return;
  }
  bool anyOn = false;
  for (const auto& device : devices) {
    if (mask & (uint16_t(1) << (device.channel - 1))) anyOn |= device.desiredState;
  }
  for (auto& device : devices) {
    if (mask & (uint16_t(1) << (device.channel - 1))) applyDeviceCommand(device, !anyOn, ControlSource::Physical);
  }
}

bool pressed(DebouncedButton& button) {
  const bool raw = digitalRead(button.pin);
  if (raw != button.lastRaw) {
    button.lastRaw = raw;
    button.lastChange = millis();
  }
  if (millis() - button.lastChange < BUTTON_DEBOUNCE_MS || raw == button.stable) return false;
  button.stable = raw;
  return button.stable == LOW;
}

void writeDeviceJson(JsonObject output, const DeviceState& device) {
  output["id"] = device.id;
  output["name"] = device.name;
  output["channel"] = device.channel;
  output["commandedState"] = device.desiredState;
  output["source"] = sourceName(device.source);
  output["priority"] = priorityOf(device.source);
  output["lastChangedAtMs"] = device.lastChangedAt;
  output["confirmedPhysicalState"] = false;
}

void writeEnergyJson(JsonObject output) {
  output["valid"] = energyReading.valid;
  output["timestampMs"] = energyReading.timestamp;
  if (!energyReading.valid) return;
  output["voltage"] = energyReading.voltage;
  output["current"] = energyReading.current;
  output["power"] = energyReading.power;
  output["energy"] = energyReading.energy;
  output["frequency"] = energyReading.frequency;
  output["powerFactor"] = energyReading.powerFactor;
}

ControlSource apiSourceFor(AuthSession& session) { return session.source; }

void replyControl(AsyncWebServerRequest* request, DeviceState* device, bool accepted, const char* reason = nullptr) {
  DynamicJsonDocument doc(512);
  doc["accepted"] = accepted;
  if (accepted && device) {
    doc["device"] = device->id;
    doc["state"] = device->desiredState;
    doc["source"] = sourceName(device->source);
    doc["priority"] = priorityOf(device->source);
  } else {
    doc["reason"] = reason ? reason : "REJECTED";
  }
  sendJson(request, accepted ? 200 : 409, doc);
}

bool parseJsonBody(AsyncWebServerRequest* request, DynamicJsonDocument& document) {
  if (!request->_tempObject) return false;
  return deserializeJson(document, static_cast<const char*>(request->_tempObject)) == DeserializationError::Ok;
}

void loadSavedSettings() {
  preferences.begin("intellirm", true);
  for (auto& device : devices) {
    char key[8];
    snprintf(key, sizeof(key), "d%u", device.channel);
    const String savedName = preferences.getString(key, "");
    if (savedName.length() > 0) strlcpy(device.name, savedName.c_str(), sizeof(device.name));
  }
  for (uint8_t index = 0; index < CUSTOM_GROUP_COUNT; ++index) {
    char nameKey[8], maskKey[8];
    snprintf(nameKey, sizeof(nameKey), "g%un", index);
    snprintf(maskKey, sizeof(maskKey), "g%um", index);
    const String savedName = preferences.getString(nameKey, "");
    if (savedName.length() > 0) strlcpy(customRelayGroups[index].name, savedName.c_str(), sizeof(customRelayGroups[index].name));
    customRelayGroups[index].channelMask = preferences.getUShort(maskKey, 0);
  }
  for (uint8_t index = 0; index < PHYSICAL_BUTTON_COUNT; ++index) {
    char nameKey[8], maskKey[8];
    snprintf(nameKey, sizeof(nameKey), "b%un", index);
    snprintf(maskKey, sizeof(maskKey), "b%um", index);
    const String savedName = preferences.getString(nameKey, "");
    const bool hasSavedButton = preferences.isKey(nameKey) || preferences.isKey(maskKey);
    if (hasSavedButton) {
      if (savedName.length() > 0) strlcpy(physicalButtonMappings[index].name, savedName.c_str(), sizeof(physicalButtonMappings[index].name));
      physicalButtonMappings[index].channelMask = preferences.getUShort(maskKey, physicalButtonMappings[index].channelMask);
    } else if (customRelayGroups[index].name[0] || customRelayGroups[index].channelMask) {
      // Migrate the earlier standalone group setup into the unified button/group slot.
      strlcpy(physicalButtonMappings[index].name, customRelayGroups[index].name, sizeof(physicalButtonMappings[index].name));
      physicalButtonMappings[index].channelMask = customRelayGroups[index].channelMask;
    }
  }
  for (uint8_t index = 0; index < SCHEDULE_COUNT; ++index) {
    char nameKey[8], maskKey[8], timeKey[8], finishKey[8], daysKey[8], actionKey[8], finishActionKey[8], enabledKey[8];
    snprintf(nameKey, sizeof(nameKey), "s%un", index); snprintf(maskKey, sizeof(maskKey), "s%um", index);
    snprintf(timeKey, sizeof(timeKey), "s%ut", index); snprintf(daysKey, sizeof(daysKey), "s%uw", index);
    snprintf(actionKey, sizeof(actionKey), "s%ua", index); snprintf(enabledKey, sizeof(enabledKey), "s%ue", index);
    snprintf(finishKey, sizeof(finishKey), "s%uf", index); snprintf(finishActionKey, sizeof(finishActionKey), "s%ub", index);
    const String savedName = preferences.getString(nameKey, "");
    if (savedName.length() > 0) strlcpy(schedules[index].name, savedName.c_str(), sizeof(schedules[index].name));
    schedules[index].channelMask = preferences.getUShort(maskKey, 0);
    schedules[index].startMinute = preferences.getUShort(timeKey, schedules[index].startMinute);
    schedules[index].endMinute = preferences.getUShort(finishKey, schedules[index].endMinute);
    schedules[index].weekdays = preferences.getUChar(daysKey, schedules[index].weekdays);
    schedules[index].startOn = preferences.getBool(actionKey, true);
    schedules[index].endOn = preferences.getBool(finishActionKey, false);
    schedules[index].enabled = preferences.getBool(enabledKey, false);
  }
  for (uint8_t index = 0; index < DEVICE_COUNT; ++index) {
    char timerKey[8]; snprintf(timerKey, sizeof(timerKey), "t%u", index + 1);
    timerDeadlines[index] = preferences.getULong(timerKey, 0);
  }
  preferences.end();
  // A physical button and its software group now share one saved combination.
  for (uint8_t index = 0; index < PHYSICAL_BUTTON_COUNT; ++index) {
    strlcpy(customRelayGroups[index].name, physicalButtonMappings[index].name, sizeof(customRelayGroups[index].name));
    customRelayGroups[index].channelMask = physicalButtonMappings[index].channelMask;
  }
}

bool validSettingName(const String& value, size_t maxLength) {
  if (value.isEmpty() || value.length() > maxLength) return false;
  for (size_t index = 0; index < value.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(value[index]);
    if (character < 32 || character == 127) return false;
  }
  return true;
}

bool savePhysicalButton(uint8_t index, const String& requestedName, uint16_t mask) {
  if (index >= PHYSICAL_BUTTON_COUNT) return false;
  String name = requestedName;
  name.trim();
  if (!validSettingName(name, sizeof(physicalButtonMappings[index].name) - 1)) return false;
  strlcpy(physicalButtonMappings[index].name, name.c_str(), sizeof(physicalButtonMappings[index].name));
  physicalButtonMappings[index].channelMask = mask;
  char nameKey[8], maskKey[8];
  snprintf(nameKey, sizeof(nameKey), "b%un", index);
  snprintf(maskKey, sizeof(maskKey), "b%um", index);
  preferences.begin("intellirm", false);
  preferences.putString(nameKey, physicalButtonMappings[index].name);
  preferences.putUShort(maskKey, mask);
  preferences.end();
  return true;
}

void saveSchedule(uint8_t index, const ScheduleRule& rule) {
  if (index >= SCHEDULE_COUNT) return;
  schedules[index] = rule;
  char nameKey[8], maskKey[8], timeKey[8], finishKey[8], daysKey[8], actionKey[8], finishActionKey[8], enabledKey[8];
  snprintf(nameKey, sizeof(nameKey), "s%un", index); snprintf(maskKey, sizeof(maskKey), "s%um", index);
  snprintf(timeKey, sizeof(timeKey), "s%ut", index); snprintf(daysKey, sizeof(daysKey), "s%uw", index);
  snprintf(actionKey, sizeof(actionKey), "s%ua", index); snprintf(enabledKey, sizeof(enabledKey), "s%ue", index);
  snprintf(finishKey, sizeof(finishKey), "s%uf", index); snprintf(finishActionKey, sizeof(finishActionKey), "s%ub", index);
  preferences.begin("intellirm", false);
  preferences.putString(nameKey, rule.name); preferences.putUShort(maskKey, rule.channelMask);
  preferences.putUShort(timeKey, rule.startMinute); preferences.putUShort(finishKey, rule.endMinute); preferences.putUChar(daysKey, rule.weekdays);
  preferences.putBool(actionKey, rule.startOn); preferences.putBool(finishActionKey, rule.endOn); preferences.putBool(enabledKey, rule.enabled);
  preferences.end();
}

void setTimerDeadline(uint8_t index, time_t deadline) {
  if (index >= DEVICE_COUNT) return;
  timerDeadlines[index] = deadline;
  char key[8]; snprintf(key, sizeof(key), "t%u", index + 1);
  preferences.begin("intellirm", false);
  if (deadline > 0) preferences.putULong(key, static_cast<uint32_t>(deadline));
  else preferences.remove(key);
  preferences.end();
}

bool controllerTimeReady() { return time(nullptr) > 1700000000; }

void runSchedulesAndTimers() {
  if (!controllerTimeReady()) return;
  const time_t now = time(nullptr);
  for (uint8_t index = 0; index < DEVICE_COUNT; ++index) {
    if (timerDeadlines[index] > 0 && now >= timerDeadlines[index]) {
      timerDeadlines[index] = 0;
      char key[8]; snprintf(key, sizeof(key), "t%u", index + 1);
      preferences.begin("intellirm", false); preferences.remove(key); preferences.end();
      if (devices[index].desiredState) {
        devices[index].desiredState = false;
        devices[index].source = ControlSource::Automation;
        devices[index].lastChangedAt = millis();
        transmitCommandedMask();
        addLog("automation", devices[index].id, "TIMER_OFF");
      }
    }
  }
  if (!remoteMode) return;
  struct tm localTime;
  if (!localtime_r(&now, &localTime)) return;
  const uint16_t minuteOfDay = uint16_t(localTime.tm_hour * 60 + localTime.tm_min);
  const int32_t minuteKey = int32_t(localTime.tm_yday + localTime.tm_year * 366) * 1440 + minuteOfDay;
  for (uint8_t slot = 0; slot < SCHEDULE_COUNT; ++slot) {
    const ScheduleRule& rule = schedules[slot];
    if (!rule.enabled || lastScheduleMinute[slot] == minuteKey) continue;
    const uint8_t weekdayBit = uint8_t(1) << localTime.tm_wday;
    const uint8_t previousWeekdayBit = uint8_t(1) << ((localTime.tm_wday + 6) % 7);
    const bool overnight = rule.endMinute < rule.startMinute;
    const bool isStart = rule.startMinute == minuteOfDay && (rule.weekdays & weekdayBit);
    const bool isEnd = rule.endMinute == minuteOfDay && (rule.weekdays & (overnight ? previousWeekdayBit : weekdayBit));
    if (!isStart && !isEnd) continue;
    lastScheduleMinute[slot] = minuteKey;
    const bool requestedState = isStart ? rule.startOn : rule.endOn;
    for (auto& device : devices) if (rule.channelMask & (uint16_t(1) << (device.channel - 1))) applyDeviceCommand(device, requestedState, ControlSource::Automation, true);
    addLog("automation", rule.name, isStart ? (requestedState ? "SCHEDULE_START_ON" : "SCHEDULE_START_OFF") : (requestedState ? "SCHEDULE_END_ON" : "SCHEDULE_END_OFF"));
  }
}

bool saveDeviceName(DeviceState& device, const String& requestedName) {
  String name = requestedName;
  name.trim();
  if (name.isEmpty() || name.length() >= sizeof(device.name)) return false;
  for (size_t index = 0; index < name.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(name[index]);
    if (character < 32 || character == 127) return false;
  }
  strlcpy(device.name, name.c_str(), sizeof(device.name));
  char key[8];
  snprintf(key, sizeof(key), "d%u", device.channel);
  preferences.begin("intellirm", false);
  preferences.putString(key, device.name);
  preferences.end();
  return true;
}

bool saveCustomRelayGroup(uint8_t slot, const String& requestedName, uint16_t mask) {
  String name = requestedName;
  name.trim();
  if (slot >= CUSTOM_GROUP_COUNT) return false;
  if (name.isEmpty() && mask == 0) {
    customRelayGroups[slot] = CustomRelayGroup{};
    char nameKey[8], maskKey[8];
    snprintf(nameKey, sizeof(nameKey), "g%un", slot);
    snprintf(maskKey, sizeof(maskKey), "g%um", slot);
    preferences.begin("intellirm", false);
    preferences.remove(nameKey);
    preferences.remove(maskKey);
    preferences.end();
    return true;
  }
  if (name.isEmpty() || name.length() >= sizeof(customRelayGroups[slot].name) || mask == 0) return false;
  for (size_t index = 0; index < name.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(name[index]);
    if (character < 32 || character == 127) return false;
  }
  strlcpy(customRelayGroups[slot].name, name.c_str(), sizeof(customRelayGroups[slot].name));
  customRelayGroups[slot].channelMask = mask;
  char nameKey[8], maskKey[8];
  snprintf(nameKey, sizeof(nameKey), "g%un", slot);
  snprintf(maskKey, sizeof(maskKey), "g%um", slot);
  preferences.begin("intellirm", false);
  preferences.putString(nameKey, customRelayGroups[slot].name);
  preferences.putUShort(maskKey, mask);
  preferences.end();
  return true;
}

void handleDeviceNameSave(AsyncWebServerRequest* request, const String& id) {
  AuthSession* session;
  if (!requireAuth(request, session)) return;
  if (session->source != ControlSource::Technician) {
    StaticJsonDocument<96> doc; doc["error"] = "TECHNICIAN_REQUIRED"; sendJson(request, 403, doc); return;
  }
  DeviceState* device = findDevice(id.c_str());
  DynamicJsonDocument input(256);
  if (!device) { StaticJsonDocument<96> doc; doc["error"] = "DEVICE_NOT_FOUND"; sendJson(request, 404, doc); return; }
  if (!parseJsonBody(request, input) || !input["name"].is<const char*>()) {
    StaticJsonDocument<96> doc; doc["error"] = "INVALID_JSON"; sendJson(request, 400, doc); return;
  }
  if (!saveDeviceName(*device, input["name"].as<String>())) {
    StaticJsonDocument<128> doc; doc["error"] = "INVALID_DEVICE_NAME"; doc["maxLength"] = sizeof(device->name) - 1; sendJson(request, 400, doc); return;
  }
  addLog("technician", device->id, "NAME_CHANGED");
  DynamicJsonDocument response(128); response["saved"] = true; response["id"] = device->id; response["name"] = device->name;
  sendJson(request, 200, response);
}

void handleCustomGroupSave(AsyncWebServerRequest* request, uint8_t slot) {
  AuthSession* session;
  if (!requireAuth(request, session)) return;
  if (session->source != ControlSource::Technician) {
    StaticJsonDocument<96> doc; doc["error"] = "TECHNICIAN_REQUIRED"; sendJson(request, 403, doc); return;
  }
  DynamicJsonDocument input(512);
  if (slot >= CUSTOM_GROUP_COUNT || !parseJsonBody(request, input) || !input["name"].is<const char*>() || !input["channels"].is<JsonArray>()) {
    StaticJsonDocument<96> doc; doc["error"] = "INVALID_JSON"; sendJson(request, 400, doc); return;
  }
  uint16_t mask = 0;
  for (JsonVariant channelValue : input["channels"].as<JsonArray>()) {
    const uint8_t channel = channelValue.as<uint8_t>();
    if (channel < 1 || channel > DEVICE_COUNT) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_CHANNEL"; sendJson(request, 400, doc); return; }
    mask |= uint16_t(1) << (channel - 1);
  }
  if (!saveCustomRelayGroup(slot, input["name"].as<String>(), mask)) {
    StaticJsonDocument<128> doc; doc["error"] = "INVALID_GROUP"; doc["maxLength"] = sizeof(customRelayGroups[slot].name) - 1; sendJson(request, 400, doc); return;
  }
  addLog("technician", "custom_group", "GROUP_SAVED");
  StaticJsonDocument<128> response; response["saved"] = true; response["slot"] = slot; response["name"] = customRelayGroups[slot].name; response["channelMask"] = mask;
  sendJson(request, 200, response);
}

void handleCustomGroupControl(AsyncWebServerRequest* request, uint8_t slot) {
  AuthSession* session;
  if (!requireAuth(request, session)) return;
  if (slot >= CUSTOM_GROUP_COUNT || customRelayGroups[slot].name[0] == '\0' || customRelayGroups[slot].channelMask == 0) {
    StaticJsonDocument<96> doc; doc["error"] = "GROUP_NOT_FOUND"; sendJson(request, 404, doc); return;
  }
  DynamicJsonDocument input(128);
  if (!parseJsonBody(request, input) || !input["state"].is<bool>()) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_JSON"; sendJson(request, 400, doc); return; }
  const bool state = input["state"].as<bool>();
  bool accepted = true;
  for (auto& device : devices) {
    if (customRelayGroups[slot].channelMask & (uint16_t(1) << (device.channel - 1))) accepted &= applyDeviceCommand(device, state, session->source);
  }
  DynamicJsonDocument response(192); response["accepted"] = accepted; response["name"] = customRelayGroups[slot].name; response["state"] = state;
  if (!accepted) response["reason"] = safetyInterlock ? "SAFETY_INTERLOCK" : "COMMAND_REJECTED";
  sendJson(request, accepted ? 200 : 409, response);
}

void handlePhysicalButtonSave(AsyncWebServerRequest* request) {
  AuthSession* session;
  if (!requireAuth(request, session)) return;
  if (session->source != ControlSource::Technician) {
    StaticJsonDocument<96> doc; doc["error"] = "TECHNICIAN_REQUIRED"; sendJson(request, 403, doc); return;
  }
  DynamicJsonDocument input(1536);
  if (!parseJsonBody(request, input) || !input["buttons"].is<JsonArray>() || input["buttons"].as<JsonArray>().size() != PHYSICAL_BUTTON_COUNT) {
    StaticJsonDocument<96> doc; doc["error"] = "INVALID_BUTTON_SETUP"; sendJson(request, 400, doc); return;
  }
  String names[PHYSICAL_BUTTON_COUNT];
  uint16_t masks[PHYSICAL_BUTTON_COUNT] = {};
  JsonArray buttons = input["buttons"].as<JsonArray>();
  for (uint8_t index = 0; index < PHYSICAL_BUTTON_COUNT; ++index) {
    JsonObject button = buttons[index];
    if (!button["name"].is<const char*>() || !button["channels"].is<JsonArray>()) {
      StaticJsonDocument<96> doc; doc["error"] = "INVALID_BUTTON_SETUP"; sendJson(request, 400, doc); return;
    }
    names[index] = button["name"].as<String>();
    names[index].trim();
    if (!validSettingName(names[index], sizeof(physicalButtonMappings[index].name) - 1)) {
      StaticJsonDocument<128> doc; doc["error"] = "INVALID_BUTTON_NAME"; doc["maxLength"] = sizeof(physicalButtonMappings[index].name) - 1; sendJson(request, 400, doc); return;
    }
    for (JsonVariant channelValue : button["channels"].as<JsonArray>()) {
      if (!channelValue.is<uint8_t>()) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_CHANNEL"; sendJson(request, 400, doc); return; }
      const uint8_t channel = channelValue.as<uint8_t>();
      if (channel < 1 || channel > DEVICE_COUNT) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_CHANNEL"; sendJson(request, 400, doc); return; }
      masks[index] |= uint16_t(1) << (channel - 1);
    }
  }
  for (uint8_t index = 0; index < PHYSICAL_BUTTON_COUNT; ++index) savePhysicalButton(index, names[index], masks[index]);
  addLog("technician", "physical_buttons", "BUTTONS_SAVED");
  StaticJsonDocument<128> response; response["saved"] = true; response["count"] = PHYSICAL_BUTTON_COUNT; sendJson(request, 200, response);
}

void collectJsonBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  // Keep JSON request bodies small and bounded. ESPAsyncWebServer frees _tempObject
  // when the request is destroyed.
  constexpr size_t MAX_JSON_BODY = 1024;
  if (total == 0 || total > MAX_JSON_BODY) {
    request->abort();
    return;
  }
  if (index == 0) {
    request->_tempObject = calloc(total + 1, sizeof(uint8_t));
    if (!request->_tempObject) {
      request->abort();
      return;
    }
  }
  if (request->_tempObject) {
    memcpy(static_cast<uint8_t*>(request->_tempObject) + index, data, len);
  }
}

void handleDeviceGet(AsyncWebServerRequest* request, const String& id) {
  AuthSession* session;
  if (!requireAuth(request, session)) return;
  DeviceState* device = findDevice(id.c_str());
  if (!device) {
    StaticJsonDocument<96> doc;
    doc["error"] = "DEVICE_NOT_FOUND";
    sendJson(request, 404, doc);
    return;
  }
  DynamicJsonDocument doc(512);
  writeDeviceJson(doc.to<JsonObject>(), *device);
  sendJson(request, 200, doc);
}

void handleDeviceControl(AsyncWebServerRequest* request, const String& id) {
  AuthSession* session;
  if (!requireAuth(request, session)) return;
  DeviceState* device = findDevice(id.c_str());
  DynamicJsonDocument input(256);
  if (!device) {
    replyControl(request, nullptr, false, "DEVICE_NOT_FOUND");
    return;
  }
  if (!parseJsonBody(request, input) || !input.containsKey("state")) {
    replyControl(request, device, false, "INVALID_JSON");
    return;
  }
  // The server derives source from the authenticated session; client `source` is ignored.
  const bool requestedState = input["state"].as<bool>();
  if (session->source == ControlSource::Technician && physicalAllOffLock) physicalAllOffLock = false;
  const bool accepted = applyDeviceCommand(*device, requestedState, apiSourceFor(*session));
  replyControl(request, device, accepted, safetyInterlock ? "SAFETY_INTERLOCK" : "COMMAND_REJECTED");
}

void handleSchedulesSave(AsyncWebServerRequest* request) {
  AuthSession* session;
  if (!requireAuth(request, session)) return;
  if (session->source != ControlSource::Technician) { StaticJsonDocument<96> doc; doc["error"] = "TECHNICIAN_REQUIRED"; sendJson(request, 403, doc); return; }
  DynamicJsonDocument input(2048);
  if (!parseJsonBody(request, input) || !input["schedules"].is<JsonArray>() || input["schedules"].as<JsonArray>().size() != SCHEDULE_COUNT) {
    StaticJsonDocument<96> doc; doc["error"] = "INVALID_SCHEDULES"; sendJson(request, 400, doc); return;
  }
  ScheduleRule parsed[SCHEDULE_COUNT];
  JsonArray rows = input["schedules"].as<JsonArray>();
  for (uint8_t slot = 0; slot < SCHEDULE_COUNT; ++slot) {
    JsonObject row = rows[slot];
    if (!row["name"].is<const char*>() || !row["channels"].is<JsonArray>() || !row["days"].is<JsonArray>() || !row["startTime"].is<const char*>() || !row["endTime"].is<const char*>() || !row["enabled"].is<bool>() || !row["startState"].is<bool>() || !row["endState"].is<bool>()) {
      StaticJsonDocument<96> doc; doc["error"] = "INVALID_SCHEDULE"; sendJson(request, 400, doc); return;
    }
    ScheduleRule& rule = parsed[slot];
    String name = row["name"].as<String>(); name.trim();
    if (name.length() >= sizeof(rule.name) || (row["enabled"].as<bool>() && name.isEmpty())) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_SCHEDULE_NAME"; sendJson(request, 400, doc); return; }
    strlcpy(rule.name, name.c_str(), sizeof(rule.name));
    String startText = row["startTime"].as<String>(), endText = row["endTime"].as<String>();
    int startHour = -1, startMinute = -1, endHour = -1, endMinute = -1;
    if (sscanf(startText.c_str(), "%d:%d", &startHour, &startMinute) != 2 || startHour < 0 || startHour > 23 || startMinute < 0 || startMinute > 59 || sscanf(endText.c_str(), "%d:%d", &endHour, &endMinute) != 2 || endHour < 0 || endHour > 23 || endMinute < 0 || endMinute > 59 || (startHour == endHour && startMinute == endMinute)) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_SCHEDULE_TIME"; sendJson(request, 400, doc); return; }
    rule.startMinute = uint16_t(startHour * 60 + startMinute); rule.endMinute = uint16_t(endHour * 60 + endMinute);
    rule.startOn = row["startState"].as<bool>(); rule.endOn = row["endState"].as<bool>(); rule.enabled = row["enabled"].as<bool>(); rule.weekdays = 0; rule.channelMask = 0;
    for (JsonVariant dayValue : row["days"].as<JsonArray>()) {
      if (!dayValue.is<uint8_t>() || dayValue.as<uint8_t>() > 6) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_WEEKDAY"; sendJson(request, 400, doc); return; }
      rule.weekdays |= uint8_t(1) << dayValue.as<uint8_t>();
    }
    for (JsonVariant channelValue : row["channels"].as<JsonArray>()) {
      if (!channelValue.is<uint8_t>() || channelValue.as<uint8_t>() < 1 || channelValue.as<uint8_t>() > DEVICE_COUNT) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_CHANNEL"; sendJson(request, 400, doc); return; }
      rule.channelMask |= uint16_t(1) << (channelValue.as<uint8_t>() - 1);
    }
    if (rule.enabled && (rule.channelMask == 0 || rule.weekdays == 0)) { StaticJsonDocument<96> doc; doc["error"] = "SCHEDULE_REQUIRES_DAYS_AND_CHANNELS"; sendJson(request, 400, doc); return; }
  }
  for (uint8_t slot = 0; slot < SCHEDULE_COUNT; ++slot) saveSchedule(slot, parsed[slot]);
  addLog("technician", "schedules", "SCHEDULES_SAVED");
  StaticJsonDocument<96> response; response["saved"] = true; response["count"] = SCHEDULE_COUNT; sendJson(request, 200, response);
}

void handleTimerSet(AsyncWebServerRequest* request, uint8_t channel) {
  AuthSession* session;
  if (!requireAuth(request, session)) return;
  if (channel < 1 || channel > DEVICE_COUNT) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_CHANNEL"; sendJson(request, 400, doc); return; }
  DynamicJsonDocument input(128);
  if (!parseJsonBody(request, input) || !input["seconds"].is<int>()) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_JSON"; sendJson(request, 400, doc); return; }
  const int seconds = input["seconds"].as<int>();
  if (seconds < 0 || seconds > 86400) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_TIMER"; sendJson(request, 400, doc); return; }
  const uint8_t index = channel - 1;
  if (seconds == 0) {
    setTimerDeadline(index, 0);
    StaticJsonDocument<128> response; response["saved"] = true; response["channel"] = channel; response["remainingSeconds"] = 0; sendJson(request, 200, response); return;
  }
  if (!controllerTimeReady()) { StaticJsonDocument<128> doc; doc["error"] = "TIME_NOT_SYNCED"; sendJson(request, 503, doc); return; }
  if (!applyDeviceCommand(devices[index], true, session->source)) { StaticJsonDocument<128> doc; doc["error"] = safetyInterlock ? "SAFETY_INTERLOCK" : "COMMAND_REJECTED"; sendJson(request, 409, doc); return; }
  const time_t deadline = time(nullptr) + time_t(seconds);
  setTimerDeadline(index, deadline);
  addLog(sourceName(session->source), devices[index].id, "TIMER_STARTED");
  StaticJsonDocument<128> response; response["saved"] = true; response["channel"] = channel; response["remainingSeconds"] = seconds; sendJson(request, 200, response);
}

void configureApi() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse_P(
      200, "text/html", DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN
    );
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  server.on("/api/v1/auth/login", HTTP_POST, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument input(256);
    if (!parseJsonBody(request, input)) {
      StaticJsonDocument<96> doc; doc["error"] = "INVALID_JSON"; sendJson(request, 400, doc); return;
    }
    const char* username = input["username"] | "";
    const char* password = input["password"] | "";
    ControlSource role;
    if (strcmp(username, TECHNICIAN_USERNAME) == 0 && strcmp(password, TECHNICIAN_PASSWORD) == 0) role = ControlSource::Technician;
    else if (strcmp(username, TEACHER_USERNAME) == 0 && strcmp(password, TEACHER_PASSWORD) == 0) role = ControlSource::Teacher;
    else {
      StaticJsonDocument<96> doc; doc["error"] = "INVALID_CREDENTIALS"; sendJson(request, 401, doc); return;
    }
    AuthSession* session = &sessions[0];
    for (auto& candidate : sessions) {
      if (!candidate.active || int32_t(candidate.expiresAt - millis()) <= 0) {
        session = &candidate;
        break;
      }
    }
    char token[17];
    snprintf(token, sizeof(token), "%08lX%08lX", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    *session = {true, String(token), role, millis() + SESSION_TTL_MS};
    DynamicJsonDocument doc(192);
    doc["authenticated"] = true;
    doc["role"] = sourceName(role);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", jsonResponse(doc));
    response->addHeader("Set-Cookie", "session=" + session->token + "; HttpOnly; SameSite=Strict; Path=/");
    request->send(response);
  }, nullptr, collectJsonBody);

  server.on("/api/v1/auth/logout", HTTP_POST, [](AsyncWebServerRequest* request) {
    AuthSession* session = authenticatedSession(request);
    if (session) *session = AuthSession{};
    StaticJsonDocument<96> doc;
    doc["authenticated"] = false;
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", jsonResponse(doc));
    response->addHeader("Set-Cookie", "session=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/");
    request->send(response);
  });

  server.on("/api/v1/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(768);
    doc["controllerOnline"] = true;
    doc["ip"] = WiFi.localIP().toString();
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["timeSynced"] = controllerTimeReady();
    doc["rssi"] = WiFi.RSSI();
    doc["mode"] = remoteMode ? "remote" : "manual";
    doc["physicalAllOffLock"] = physicalAllOffLock;
    doc["safetyInterlock"] = safetyInterlock;
    doc["commandedOnCount"] = commandedOnCount();
    doc["relayMask"] = commandedRelayMask();
    doc["relayFeedbackAvailable"] = false;
    doc["role"] = sourceName(session->source);
    writeEnergyJson(doc.createNestedObject("energy"));
    sendJson(request, 200, doc);
  });

  server.on("/api/v1/devices", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(4096);
    JsonArray list = doc.to<JsonArray>();
    for (const auto& device : devices) writeDeviceJson(list.createNestedObject(), device);
    sendJson(request, 200, doc);
  });

  auto groupHandler = [](uint8_t first, uint8_t last) {
    return [first, last](AsyncWebServerRequest* request) {
      AuthSession* session; if (!requireAuth(request, session)) return;
      DynamicJsonDocument input(128);
      if (!parseJsonBody(request, input) || !input.containsKey("state")) { replyControl(request, nullptr, false, "INVALID_JSON"); return; }
      const bool state = input["state"].as<bool>();
      bool accepted = true;
      for (uint8_t index = first; index <= last; ++index) accepted &= applyDeviceCommand(devices[index], state, session->source);
      DynamicJsonDocument doc(192); doc["accepted"] = accepted; doc["group"] = first == 0 ? "lights" : "fans"; doc["state"] = state;
      if (!accepted) doc["reason"] = safetyInterlock ? "SAFETY_INTERLOCK" : "COMMAND_REJECTED";
      sendJson(request, accepted ? 200 : 409, doc);
    };
  };
  server.on("/api/v1/groups/lights", HTTP_POST, groupHandler(0, 3), nullptr, collectJsonBody);
  server.on("/api/v1/groups/fans", HTTP_POST, groupHandler(4, 7), nullptr, collectJsonBody);

  server.on("/api/v1/system/all-on", HTTP_POST, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    bool accepted = !safetyInterlock;
    if (session->source == ControlSource::Technician) physicalAllOffLock = false;
    for (auto& device : devices) accepted &= applyDeviceCommand(device, true, session->source);
    DynamicJsonDocument doc(128); doc["accepted"] = accepted; doc["commandedOnCount"] = commandedOnCount();
    if (!accepted) doc["reason"] = safetyInterlock ? "SAFETY_INTERLOCK" : "COMMAND_REJECTED";
    sendJson(request, accepted ? 200 : 409, doc);
  }, nullptr, collectJsonBody);

  // Register exact device URLs so the server's request-body callback is always
  // attached for individual relay commands and technician name edits.
  for (const auto& device : devices) {
    const String deviceId(device.id);
    const String controlUri = String("/api/v1/devices/") + deviceId + "/control";
    const String nameUri = String("/api/v1/devices/") + deviceId + "/name";
    const String timerUri = String("/api/v1/timers/") + String(static_cast<unsigned int>(device.channel));
    server.on(controlUri, HTTP_POST, [deviceId](AsyncWebServerRequest* request) { handleDeviceControl(request, deviceId); }, nullptr, collectJsonBody);
    server.on(nameUri, HTTP_POST, [deviceId](AsyncWebServerRequest* request) { handleDeviceNameSave(request, deviceId); }, nullptr, collectJsonBody);
    server.on(timerUri, HTTP_POST, [channel = device.channel](AsyncWebServerRequest* request) { handleTimerSet(request, channel); }, nullptr, collectJsonBody);
  }

  server.on("/api/v1/groups/custom", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(1024);
    JsonArray result = doc.to<JsonArray>();
    for (uint8_t slot = 0; slot < CUSTOM_GROUP_COUNT; ++slot) {
      JsonObject group = result.createNestedObject();
      group["slot"] = slot;
      group["name"] = customRelayGroups[slot].name;
      JsonArray channels = group.createNestedArray("channels");
      for (const auto& device : devices) {
        if (customRelayGroups[slot].channelMask & (uint16_t(1) << (device.channel - 1))) channels.add(device.channel);
      }
    }
    sendJson(request, 200, doc);
  });
  for (uint8_t slot = 0; slot < CUSTOM_GROUP_COUNT; ++slot) {
    const String saveUri = String("/api/v1/groups/custom/") + slot;
    const String controlUri = saveUri + "/control";
    server.on(saveUri, HTTP_POST, [slot](AsyncWebServerRequest* request) { handleCustomGroupSave(request, slot); }, nullptr, collectJsonBody);
    server.on(controlUri, HTTP_POST, [slot](AsyncWebServerRequest* request) { handleCustomGroupControl(request, slot); }, nullptr, collectJsonBody);
  }

  server.on("/api/v1/buttons/custom", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(1024);
    JsonArray result = doc.to<JsonArray>();
    for (uint8_t index = 0; index < PHYSICAL_BUTTON_COUNT; ++index) {
      JsonObject button = result.createNestedObject();
      button["index"] = index;
      button["pin"] = physicalButtons[index].pin;
      button["name"] = physicalButtonMappings[index].name;
      JsonArray channels = button.createNestedArray("channels");
      for (const auto& device : devices) {
        if (physicalButtonMappings[index].channelMask & (uint16_t(1) << (device.channel - 1))) channels.add(device.channel);
      }
    }
    sendJson(request, 200, doc);
  });
  server.on("/api/v1/buttons/custom", HTTP_POST, handlePhysicalButtonSave, nullptr, collectJsonBody);

  server.on("/api/v1/schedules", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(2048); JsonArray result = doc.to<JsonArray>();
    for (uint8_t slot = 0; slot < SCHEDULE_COUNT; ++slot) {
      const ScheduleRule& rule = schedules[slot];
      JsonObject row = result.createNestedObject(); row["slot"] = slot; row["name"] = rule.name;
      char startText[6], endText[6]; snprintf(startText, sizeof(startText), "%02u:%02u", rule.startMinute / 60, rule.startMinute % 60); snprintf(endText, sizeof(endText), "%02u:%02u", rule.endMinute / 60, rule.endMinute % 60); row["startTime"] = startText; row["endTime"] = endText;
      row["startState"] = rule.startOn; row["endState"] = rule.endOn; row["enabled"] = rule.enabled;
      JsonArray days = row.createNestedArray("days"); for (uint8_t day = 0; day < 7; ++day) if (rule.weekdays & (uint8_t(1) << day)) days.add(day);
      JsonArray channels = row.createNestedArray("channels"); for (const auto& device : devices) if (rule.channelMask & (uint16_t(1) << (device.channel - 1))) channels.add(device.channel);
    }
    sendJson(request, 200, doc);
  });
  server.on("/api/v1/schedules", HTTP_POST, handleSchedulesSave, nullptr, collectJsonBody);
  server.on("/api/v1/timers", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(1024); JsonArray result = doc.to<JsonArray>();
    const time_t now = controllerTimeReady() ? time(nullptr) : 0;
    for (uint8_t index = 0; index < DEVICE_COUNT; ++index) {
      JsonObject timer = result.createNestedObject(); timer["channel"] = index + 1;
      const time_t remaining = timerDeadlines[index] > now ? timerDeadlines[index] - now : 0;
      timer["remainingSeconds"] = static_cast<uint32_t>(remaining);
    }
    sendJson(request, 200, doc);
  });

  server.on("/api/v1/system/all-off", HTTP_POST, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    forceAllOff(session->source, false);
    StaticJsonDocument<96> doc; doc["accepted"] = true; doc["commandedOnCount"] = 0; sendJson(request, 200, doc);
  });

  server.on("/api/v1/energy/live", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(512); writeEnergyJson(doc.to<JsonObject>()); sendJson(request, 200, doc);
  });

  server.on("/api/v1/mode", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    StaticJsonDocument<128> doc; doc["mode"] = remoteMode ? "remote" : "manual"; doc["automationEnabled"] = remoteMode && !physicalAllOffLock && !safetyInterlock; sendJson(request, 200, doc);
  });

  server.on("/api/v1/mode", HTTP_POST, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    if (session->source != ControlSource::Technician) { StaticJsonDocument<96> doc; doc["error"] = "TECHNICIAN_REQUIRED"; sendJson(request, 403, doc); return; }
    DynamicJsonDocument input(128);
    if (!parseJsonBody(request, input) || !input.containsKey("mode")) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_JSON"; sendJson(request, 400, doc); return; }
    const char* mode = input["mode"];
    if (strcmp(mode, "remote") != 0 && strcmp(mode, "manual") != 0) { StaticJsonDocument<96> doc; doc["error"] = "INVALID_MODE"; sendJson(request, 400, doc); return; }
    const bool requestedRemoteMode = strcmp(mode, "remote") == 0;
    // GPIO27 is a physical two-position switch, so it is authoritative. Do not
    // acknowledge a remote mode change that the next loop would immediately undo.
    if (requestedRemoteMode != (digitalRead(MODE_TOGGLE_PIN) == HIGH)) {
      StaticJsonDocument<128> doc;
      doc["error"] = "PHYSICAL_MODE_SWITCH_AUTHORITATIVE";
      doc["mode"] = remoteMode ? "remote" : "manual";
      sendJson(request, 409, doc);
      return;
    }
    remoteMode = requestedRemoteMode;
    addLog("technician", "mode", remoteMode ? "REMOTE" : "MANUAL");
    StaticJsonDocument<96> doc; doc["mode"] = remoteMode ? "remote" : "manual"; sendJson(request, 200, doc);
  }, nullptr, collectJsonBody);

  server.on("/api/v1/system", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(512);
    doc["uptimeMs"] = millis(); doc["freeHeap"] = ESP.getFreeHeap(); doc["ip"] = WiFi.localIP().toString();
    doc["wifiRssi"] = WiFi.RSSI(); doc["pzemOnline"] = energyReading.valid;
    doc["relayCommandLink"] = "configured_one_way"; doc["lcdConfigured"] = true;
    sendJson(request, 200, doc);
  });

  server.on("/api/v1/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(4096); JsonArray list = doc.to<JsonArray>();
    const uint8_t start = (logWriteIndex + LOG_CAPACITY - logCount) % LOG_CAPACITY;
    for (uint8_t i = 0; i < logCount; ++i) {
      const ActivityLog& entry = activityLog[(start + i) % LOG_CAPACITY];
      JsonObject row = list.createNestedObject(); row["timestampMs"] = entry.timestampMs; row["source"] = entry.source; row["target"] = entry.target; row["action"] = entry.action;
    }
    sendJson(request, 200, doc);
  });

  server.on("/api/v1/alerts", HTTP_GET, [](AsyncWebServerRequest* request) {
    AuthSession* session; if (!requireAuth(request, session)) return;
    DynamicJsonDocument doc(512); JsonArray alerts = doc.to<JsonArray>();
    if (!energyReading.valid) { JsonObject alert = alerts.createNestedObject(); alert["severity"] = "WARNING"; alert["message"] = "PZEM unavailable"; }
    if (physicalAllOffLock) { JsonObject alert = alerts.createNestedObject(); alert["severity"] = "INFO"; alert["message"] = "ALL OFF lock active"; }
    if (!remoteMode) { JsonObject alert = alerts.createNestedObject(); alert["severity"] = "INFO"; alert["message"] = "Manual mode enabled"; }
    if (safetyInterlock) { JsonObject alert = alerts.createNestedObject(); alert["severity"] = "CRITICAL"; alert["message"] = "Safety interlock active"; }
    sendJson(request, 200, doc);
  });

  server.onNotFound([](AsyncWebServerRequest* request) {
    const String prefix = "/api/v1/devices/";
    const String url = request->url();
    if (url.startsWith(prefix)) {
      String suffix = url.substring(prefix.length());
      const String controlSuffix = "/control";
      if (request->method() == HTTP_POST && suffix.endsWith(controlSuffix)) {
        suffix.remove(suffix.length() - controlSuffix.length());
        if (suffix.length() > 0 && suffix.indexOf('/') < 0) {
          handleDeviceControl(request, suffix);
          return;
        }
      } else if (request->method() == HTTP_GET && suffix.length() > 0 && suffix.indexOf('/') < 0) {
        handleDeviceGet(request, suffix);
        return;
      }
    }
    StaticJsonDocument<96> doc; doc["error"] = "NOT_FOUND"; sendJson(request, 404, doc);
  });
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== IntelliRoom ESP32 starting ===");
  loadSavedSettings();
  Serial.print("Wi-Fi SSID: ");
  Serial.println(WIFI_SSID);
  if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) {
    Serial.println("ERROR: Set WIFI_SSID and WIFI_PASSWORD in config.h, then upload again.");
  }
  relaySerial.begin(UNO_BAUD, SERIAL_8N1, -1, UNO_TX_PIN);
  transmitCommandedMask(); // Mandatory safe start: ESP32 commands all relays off.

  pinMode(LIGHTS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FANS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ALL_ON_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ALL_OFF_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MODE_TOGGLE_PIN, INPUT_PULLUP);
  remoteMode = digitalRead(MODE_TOGGLE_PIN) == HIGH;

  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init(); lcd.backlight(); lcd.clear();
  renderStatusLcd(5); // Show the button diagnostic immediately after boot.

  Serial2.begin(PZEM_BAUD, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTime(TIMEZONE_OFFSET_SECONDS, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Connecting to Wi-Fi...");
  configureApi();
  server.begin();
  Serial.println("HTTP API started on port 80.");
  addLog("system", "controller", "BOOT");
}

void loop() {
  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected && !wifiWasConnected) {
    Serial.println("Wi-Fi connected.");
    Serial.print("ESP32 IP address: http://");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else if (!wifiConnected && wifiWasConnected) {
    Serial.println("Wi-Fi disconnected; reconnecting...");
  }
  wifiWasConnected = wifiConnected;

  const bool newRemoteMode = digitalRead(MODE_TOGGLE_PIN) == HIGH;
  if (newRemoteMode != remoteMode) {
    remoteMode = newRemoteMode;
    addLog("physical", "mode", remoteMode ? "REMOTE" : "MANUAL");
  }
  for (uint8_t index = 0; index < PHYSICAL_BUTTON_COUNT; ++index) {
    if (pressed(physicalButtons[index])) {
      Serial.printf("Button %u pressed on GPIO %u; group mask 0x%02X.\n", index + 1, physicalButtons[index].pin, physicalButtonMappings[index].channelMask);
      togglePhysicalButton(index);
    }
  }
  runSchedulesAndTimers();

  if (millis() - lastEnergyPoll >= ENERGY_POLL_INTERVAL_MS) {
    lastEnergyPoll = millis();
    const bool pzemOnline = readEnergy(pzem);
    if (pzemOnline != pzemWasOnline) addLog("system", "pzem", pzemOnline ? "CONNECTED" : "UNAVAILABLE");
    pzemWasOnline = pzemOnline;
  }
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiRetry > 10000) {
    lastWifiRetry = millis();
    Serial.print("Wi-Fi still not connected; status code: ");
    Serial.println(static_cast<int>(WiFi.status()));
    WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  if (millis() - lastLcdDraw >= 500) {
    lastLcdDraw = millis();
    if (millis() < 30000) {
      lcdPage = 5; // Keep the button diagnostic visible for the first 30 seconds.
    } else {
      lcdPage = ((millis() - 30000) / 5000) % 5;
    }
    renderStatusLcd(lcdPage);
  }
}
