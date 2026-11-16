// Standalone button wiring check for the IntelliRoom ESP32 Dev Module.
// Wire each momentary button between its GPIO and GND.

constexpr uint8_t BUTTON_PINS[] = {32, 33, 25, 26};
constexpr uint8_t BUTTON_COUNT = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);
bool lastState[BUTTON_COUNT];

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("IntelliRoom button pin check");
  Serial.println("Release = HIGH; press = LOW");

  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    lastState[i] = digitalRead(BUTTON_PINS[i]);
    Serial.printf("GPIO %u initial: %s\n", BUTTON_PINS[i], lastState[i] ? "HIGH (released)" : "LOW (pressed)");
  }
}

void loop() {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    const bool reading = digitalRead(BUTTON_PINS[i]);
    if (reading == lastState[i]) continue;

    delay(25); // Simple debounce for this diagnostic only.
    const bool stableReading = digitalRead(BUTTON_PINS[i]);
    if (stableReading == lastState[i]) continue;

    lastState[i] = stableReading;
    Serial.printf("GPIO %u: %s\n", BUTTON_PINS[i], stableReading ? "RELEASED (HIGH)" : "PRESSED (LOW)");
  }
  delay(5);
}
