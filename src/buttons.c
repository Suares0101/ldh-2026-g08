#include <Arduino.h>
#include "buttons.h"
#include "config.h"

namespace {

struct ButtonState {
  bool stableHigh;
  bool lastReadingHigh;
  uint32_t changedAtMs;
};

ButtonState buttons[BUZZER_COUNT] = {};

}

void buttonsInit() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < BUZZER_COUNT; ++i) {
    const bool reading = digitalRead(BUTTON_PINS[i]) == HIGH;
    buttons[i].stableHigh = reading;
    buttons[i].lastReadingHigh = reading;
    buttons[i].changedAtMs = now;
  }
}

bool buttonPressed(uint8_t index) {
  if (index >= BUZZER_COUNT) {
    return false;
  }

  const bool readingHigh = digitalRead(BUTTON_PINS[index]) == HIGH;
  const uint32_t now = millis();

  if (readingHigh != buttons[index].lastReadingHigh) {
    buttons[index].lastReadingHigh = readingHigh;
    buttons[index].changedAtMs = now;
  }

  if (static_cast<uint32_t>(now - buttons[index].changedAtMs) < 30 ||
      readingHigh == buttons[index].stableHigh) {
    return false;
  }

  buttons[index].stableHigh = readingHigh;
  return !readingHigh;
}
