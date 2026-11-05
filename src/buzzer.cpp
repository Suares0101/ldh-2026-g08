#include <Arduino.h>
#include "buzzer.h"
#include "config.h"

namespace {

struct BuzzerState {
  bool active;
  bool outputHigh;
  uint16_t halfPeriodUs;
  uint32_t nextToggleUs;
  uint32_t startedAtMs;
  uint16_t durationMs;
};

BuzzerState buzzers[BUZZER_COUNT] = {};

const uint16_t NOTE_FREQUENCIES[MIDI_MAX_NOTE - MIDI_MIN_NOTE + 1] = {
  262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494,
  523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988,
  1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976
};

}

void buzzerInit() {
  for (uint8_t i = 0; i < BUZZER_COUNT; ++i) {
    pinMode(BUZZER_PINS[i], OUTPUT);
    digitalWrite(BUZZER_PINS[i], LOW);
  }
}

void buzzerStop(uint8_t index) {
  if (index >= BUZZER_COUNT) {
    return;
  }

  buzzers[index].active = false;
  buzzers[index].outputHigh = false;
  digitalWrite(BUZZER_PINS[index], LOW);
}

void buzzerStopAll() {
  for (uint8_t i = 0; i < BUZZER_COUNT; ++i) {
    buzzerStop(i);
  }
}

void buzzerStart(uint8_t index, uint16_t frequency, uint16_t durationMs) {
  if (index >= BUZZER_COUNT || frequency == 0 || durationMs == 0) {
    if (index < BUZZER_COUNT) {
      buzzerStop(index);
    }
    return;
  }

  BuzzerState &buzzer = buzzers[index];
  buzzer.active = true;
  buzzer.outputHigh = false;
  buzzer.halfPeriodUs = static_cast<uint16_t>(500000UL / frequency);
  buzzer.startedAtMs = millis();
  buzzer.durationMs = durationMs;
  buzzer.nextToggleUs = micros();
  digitalWrite(BUZZER_PINS[index], LOW);
}

// Atualiza cada buzzer em separado e mantém a fase para reduzir desvios de frequência.
void buzzerUpdate() {
  const uint32_t nowMs = millis();
  const uint32_t nowUs = micros();

  for (uint8_t i = 0; i < BUZZER_COUNT; ++i) {
    BuzzerState &buzzer = buzzers[i];
    if (!buzzer.active) {
      continue;
    }

    if (static_cast<uint32_t>(nowMs - buzzer.startedAtMs) >= buzzer.durationMs) {
      buzzerStop(i);
      continue;
    }

    if (static_cast<int32_t>(nowUs - buzzer.nextToggleUs) >= 0) {
      const uint32_t elapsedPeriods =
          static_cast<uint32_t>(nowUs - buzzer.nextToggleUs) / buzzer.halfPeriodUs + 1;
      if ((elapsedPeriods & 1) != 0) {
        buzzer.outputHigh = !buzzer.outputHigh;
        digitalWrite(BUZZER_PINS[i], buzzer.outputHigh ? HIGH : LOW);
      }
      buzzer.nextToggleUs += elapsedPeriods * buzzer.halfPeriodUs;
    }
  }
}

uint16_t midiNoteFrequency(uint8_t note) {
  if (note < MIDI_MIN_NOTE || note > MIDI_MAX_NOTE) {
    return 0;
  }
  return NOTE_FREQUENCIES[note - MIDI_MIN_NOTE];
}
