#include <Arduino.h>
#include <EEPROM.h>
#include "buzzer.h"
#include "config.h"
#include "presets.h"

namespace {

struct VoiceState {
  uint8_t scanIndex;
  bool waiting;
  bool finished;
  uint32_t eventStartedAtMs;
  uint16_t eventDurationMs;
};

VoiceState voices[BUZZER_COUNT] = {};
bool playing = false;
uint8_t playingSlot = 0;
uint8_t eventCount = 0;

uint16_t slotAddress(uint8_t slot) {
  return static_cast<uint16_t>(slot - 1) * SLOT_SIZE;
}

bool validPreset(uint8_t slot, uint8_t &count) {
  if (slot < 1 || slot > 2) {
    return false;
  }

  const uint16_t address = slotAddress(slot);
  if (EEPROM.read(address) != EEPROM_MAGIC) {
    return false;
  }

  count = EEPROM.read(address + 1);
  return count > 0 && count <= MAX_EVENTS;
}

bool readEvent(uint8_t index, uint8_t &channel, uint8_t &note,
               uint8_t &duration) {
  if (index >= eventCount) {
    return false;
  }

  const uint16_t address = slotAddress(playingSlot) + 2 +
                           static_cast<uint16_t>(index) * EVENT_SIZE;
  channel = EEPROM.read(address);
  note = EEPROM.read(address + 1);
  duration = EEPROM.read(address + 2);
  return channel >= 1 && channel <= BUZZER_COUNT &&
         (note == 0 || (note >= MIDI_MIN_NOTE && note <= MIDI_MAX_NOTE)) &&
         duration >= MIDI_MIN_DURATION && duration <= MIDI_MAX_DURATION;
}

// Cada voz procura os seus eventos sem bloquear as restantes.
void scheduleNextEvent(uint8_t voiceIndex, uint32_t nowMs) {
  VoiceState &voice = voices[voiceIndex];

  while (voice.scanIndex < eventCount) {
    uint8_t channel;
    uint8_t note;
    uint8_t duration;
    const uint8_t index = voice.scanIndex++;

    if (!readEvent(index, channel, note, duration)) {
      voice.finished = true;
      buzzerStop(voiceIndex);
      return;
    }
    if (channel != voiceIndex + 1) {
      continue;
    }

    voice.waiting = true;
    voice.eventStartedAtMs = nowMs;
    voice.eventDurationMs = static_cast<uint16_t>(duration) * 10;
    buzzerStart(voiceIndex, midiNoteFrequency(note), voice.eventDurationMs);
    return;
  }

  voice.finished = true;
  voice.waiting = false;
  buzzerStop(voiceIndex);
}

}

void presetStopPlayback() {
  playing = false;
}

void presetPlay(uint8_t slot) {
  playing = false;
  buzzerStopAll();

  uint8_t count;
  if (!validPreset(slot, count)) {
    Serial.print(F("ERRO: musica "));
    Serial.print(slot);
    Serial.println(F(" vazia ou invalida"));
    return;
  }

  playingSlot = slot;
  eventCount = count;
  playing = true;

  for (uint8_t i = 0; i < BUZZER_COUNT; ++i) {
    voices[i].scanIndex = 0;
    voices[i].waiting = false;
    voices[i].finished = false;
    scheduleNextEvent(i, millis());
  }

  Serial.print(F("A tocar musica "));
  Serial.println(slot);
}

void presetUpdate() {
  if (!playing) {
    return;
  }

  const uint32_t now = millis();
  bool allFinished = true;

  for (uint8_t i = 0; i < BUZZER_COUNT; ++i) {
    VoiceState &voice = voices[i];
    if (voice.finished) {
      continue;
    }
    allFinished = false;

    if (voice.waiting &&
        static_cast<uint32_t>(now - voice.eventStartedAtMs) >= voice.eventDurationMs) {
      voice.waiting = false;
      buzzerStop(i);
    }
    if (!voice.waiting) {
      scheduleNextEvent(i, now);
    }
  }

  for (uint8_t i = 0; i < BUZZER_COUNT; ++i) {
    if (!voices[i].finished) {
      allFinished = false;
    }
  }
  if (allFinished) {
    playing = false;
  }
}
