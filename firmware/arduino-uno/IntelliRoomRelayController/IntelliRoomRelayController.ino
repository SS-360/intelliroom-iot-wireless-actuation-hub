#include "protocol.h"

// The Uno has no policy, Wi-Fi, priority, or web logic. It only applies commands.
constexpr uint32_t SERIAL_BAUD = 9600;
char commandBuffer[24];
uint8_t commandLength = 0;

void safeStartup() {
  // Set the inactive level before making each pin an output to minimise relay glitches.
  for (uint8_t channel = 0; channel < RELAY_COUNT; ++channel) {
    const uint8_t offLevel = RELAY_ACTIVE_LOW ? HIGH : LOW;
    digitalWrite(RELAY_PINS[channel], offLevel);
    pinMode(RELAY_PINS[channel], OUTPUT);
  }
  applyRelayMask(0x0000);
}

bool parseMask(const char* value, uint16_t& mask) {
  if (strlen(value) != 4) return false;
  char* end = nullptr;
  const unsigned long parsed = strtoul(value, &end, 16);
  if (*end != '\0' || parsed > 0xFFFFUL) return false;
  mask = static_cast<uint16_t>(parsed);
  return true;
}

void processCommand(char* command) {
  // Canonical command: M,FFFF
  if (strncmp(command, "M,", 2) == 0) {
    uint16_t mask;
    if (parseMask(command + 2, mask)) applyRelayMask(mask);
    return;
  }

  // Optional bench-test commands. They are deliberately not required by ESP32.
  if (strncmp(command, "ALL,", 4) == 0) {
    if (command[4] == '1' && command[5] == '\0') applyRelayMask(0xFFFF);
    if (command[4] == '0' && command[5] == '\0') applyRelayMask(0x0000);
    return;
  }

  if (strncmp(command, "R,", 2) == 0) {
    char* save = nullptr;
    strtok_r(command, ",", &save); // R
    char* channelText = strtok_r(nullptr, ",", &save);
    char* stateText = strtok_r(nullptr, ",", &save);
    if (!channelText || !stateText || strtok_r(nullptr, ",", &save)) return;
    const long channel = strtol(channelText, nullptr, 10);
    if (channel < 1 || channel > RELAY_COUNT) return;
    if (strcmp(stateText, "1") == 0) writeRelay(channel - 1, true);
    if (strcmp(stateText, "0") == 0) writeRelay(channel - 1, false);
  }
}

void setup() {
  safeStartup();
  Serial.begin(SERIAL_BAUD);
}

void loop() {
  while (Serial.available()) {
    const char received = static_cast<char>(Serial.read());
    if (received == '\r') continue;
    if (received == '\n') {
      commandBuffer[commandLength] = '\0';
      if (commandLength > 0) processCommand(commandBuffer);
      commandLength = 0;
      continue;
    }
    if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = received;
    } else {
      // Drop an overlong/corrupt command until its newline arrives.
      commandLength = 0;
    }
  }
}
