#include <Arduino.h>
#include "buttons.h"
#include "buzzer.h"
#include "config.h"
#include "presets.h"
#include "serial_modes.h"

namespace {

uint8_t currentMode = MODE_PRESETS;

void setMode(uint8_t mode) {
  currentMode = mode;
  presetStopPlayback();
  buzzerStopAll();
  serialModesReset();

  for (uint8_t i = 0; i < BUZZER_COUNT; ++i) {
    digitalWrite(LED_PINS[i], i + 1 == currentMode ? HIGH : LOW);
  }

  Serial.print(F("Modo "));
  Serial.println(currentMode);
}

}

void setup() {
  buzzerInit();
  for (uint8_t i = 0; i < BUZZER_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  Serial.begin(9600);
  buttonsInit();
  setMode(MODE_PRESETS);
  Serial.println(F("Pronto: B3 muda de modo; modo 2 usa B1_300_250."));
}

void loop() {
  if (buttonPressed(2)) {
    const uint8_t nextMode =
        currentMode == MODE_RECORD ? MODE_PRESETS : currentMode + 1;
    setMode(nextMode);
  }

  if (currentMode == MODE_PRESETS) {
    if (buttonPressed(0)) {
      presetPlay(1);
    }
    if (buttonPressed(1)) {
      presetPlay(2);
    }
    presetUpdate();
  } else if (currentMode == MODE_LIVE) {
    serialModeLiveUpdate();
  } else if (currentMode == MODE_RECORD) {
    serialModeRecordUpdate();
  }

  buzzerUpdate();
}
