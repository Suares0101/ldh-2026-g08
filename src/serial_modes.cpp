#include <Arduino.h>
#include <EEPROM.h>
#include "buzzer.h"
#include "config.h"
#include "serial_modes.h"

namespace {

enum MidiReceiveState {
  MIDI_WAIT_HEADER,
  MIDI_WAIT_SLOT,
  MIDI_WAIT_LENGTH,
  MIDI_WAIT_EVENTS,
  MIDI_WAIT_CHECKSUM
};

uint8_t midiData[MAX_FRAME_DATA];
MidiReceiveState midiState = MIDI_WAIT_HEADER;
uint8_t midiSlot = 0;
uint8_t midiLength = 0;
uint8_t midiDataIndex = 0;
uint8_t midiChecksum = 0;
bool midiFrameValid = true;
uint32_t midiLastByteAtMs = 0;

char liveCommand[32];
uint8_t liveCommandLength = 0;
bool liveCommandOverflow = false;

bool parseUnsigned(const char *text, uint8_t length, uint16_t &value) {
  if (length == 0) {
    return false;
  }

  uint32_t parsed = 0;
  for (uint8_t i = 0; i < length; ++i) {
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
    parsed = parsed * 10 + static_cast<uint8_t>(text[i] - '0');
    if (parsed > 65535) {
      return false;
    }
  }
  value = static_cast<uint16_t>(parsed);
  return true;
}

bool parseLiveCommand(const char *command, uint8_t length, uint8_t &channel,
                      uint16_t &frequency, uint16_t &duration) {
  if (length < 7 || command[0] != 'B') {
    return false;
  }

  uint8_t firstSeparator = 0;
  uint8_t secondSeparator = 0;
  for (uint8_t i = 1; i < length; ++i) {
    if (command[i] == '_') {
      if (firstSeparator == 0) {
        firstSeparator = i;
      } else if (secondSeparator == 0) {
        secondSeparator = i;
      } else {
        return false;
      }
    }
  }
  if (firstSeparator == 0 || secondSeparator == 0 ||
      secondSeparator <= firstSeparator + 1 || secondSeparator + 1 >= length) {
    return false;
  }

  uint16_t buzzerNumber;
  if (!parseUnsigned(command + 1, firstSeparator - 1, buzzerNumber) ||
      !parseUnsigned(command + firstSeparator + 1,
                     secondSeparator - firstSeparator - 1, frequency) ||
      !parseUnsigned(command + secondSeparator + 1,
                     length - secondSeparator - 1, duration) ||
      buzzerNumber < 1 || buzzerNumber > BUZZER_COUNT ||
      (duration != 0 &&
       (frequency < LIVE_MIN_FREQUENCY || frequency > LIVE_MAX_FREQUENCY)) ||
      (duration != 0 && (duration < 50 || duration > 2000))) {
    return false;
  }

  channel = static_cast<uint8_t>(buzzerNumber);
  return true;
}

void processLiveCommand() {
  uint8_t channel;
  uint16_t frequency;
  uint16_t duration;

  if (liveCommandOverflow ||
      !parseLiveCommand(liveCommand, liveCommandLength, channel, frequency, duration)) {
    Serial.println(F("ERRO: comando invalido (B1_300_250 ou B2_100_0)"));
  } else if (duration == 0) {
    buzzerStop(channel - 1);
    Serial.println(F("OK: pausa"));
  } else {
    buzzerStart(channel - 1, frequency, duration);
    Serial.println(F("OK"));
  }

  liveCommandLength = 0;
  liveCommandOverflow = false;
}

void receiveLiveByte(char value) {
  if (value == ' ' || value == '\t' || value == '\r' || value == '\n') {
    if (liveCommandLength > 0 || liveCommandOverflow) {
      processLiveCommand();
    }
    return;
  }

  if (liveCommandLength < sizeof(liveCommand) - 1) {
    liveCommand[liveCommandLength++] = value;
  } else {
    liveCommandOverflow = true;
  }
}

void writePresetToEeprom() {
  const uint16_t address = static_cast<uint16_t>(midiSlot - 1) * SLOT_SIZE;
  EEPROM.update(address, 0);

  for (uint16_t i = 0; i < midiLength; ++i) {
    EEPROM.update(address + 2 + i, midiData[i]);
  }
  EEPROM.update(address + 1, midiLength / EVENT_SIZE);
  EEPROM.update(address, EEPROM_MAGIC);

  Serial.print(F("OK: musica "));
  Serial.print(midiSlot);
  Serial.print(F(" gravada ("));
  Serial.print(midiLength / EVENT_SIZE);
  Serial.println(F(" eventos)"));
}

void resetMidiReceiver() {
  midiState = MIDI_WAIT_HEADER;
  midiSlot = 0;
  midiLength = 0;
  midiDataIndex = 0;
  midiChecksum = 0;
  midiFrameValid = true;
}

void receiveMidiByte(uint8_t value) {
  midiLastByteAtMs = millis();

  switch (midiState) {
    case MIDI_WAIT_HEADER:
      if (value == MIDI_HEADER) {
        midiChecksum = value;
        midiFrameValid = true;
        midiState = MIDI_WAIT_SLOT;
      }
      break;

    case MIDI_WAIT_SLOT:
      midiSlot = value;
      midiChecksum ^= value;
      if (value < 1 || value > 2) {
        midiFrameValid = false;
      }
      midiState = MIDI_WAIT_LENGTH;
      break;

    case MIDI_WAIT_LENGTH:
      midiLength = value;
      midiChecksum ^= value;
      if (value < EVENT_SIZE || value > MAX_FRAME_DATA ||
          value % EVENT_SIZE != 0) {
        Serial.println(F("ERRO: comprimento de trama invalido"));
        resetMidiReceiver();
      } else {
        midiDataIndex = 0;
        midiState = MIDI_WAIT_EVENTS;
      }
      break;

    case MIDI_WAIT_EVENTS:
      midiData[midiDataIndex++] = value;
      midiChecksum ^= value;
      if ((midiDataIndex % EVENT_SIZE) == 1 &&
          (value < 1 || value > BUZZER_COUNT)) {
        midiFrameValid = false;
      } else if ((midiDataIndex % EVENT_SIZE) == 2 &&
                 value != 0 && (value < MIDI_MIN_NOTE || value > MIDI_MAX_NOTE)) {
        midiFrameValid = false;
      } else if ((midiDataIndex % EVENT_SIZE) == 0 &&
                 (value < MIDI_MIN_DURATION || value > MIDI_MAX_DURATION)) {
        midiFrameValid = false;
      }
      if (midiDataIndex == midiLength) {
        midiState = MIDI_WAIT_CHECKSUM;
      }
      break;

    case MIDI_WAIT_CHECKSUM:
      if (value == midiChecksum && midiFrameValid) {
        writePresetToEeprom();
      } else {
        Serial.println(F("ERRO: trama MIDI invalida (dados ou checksum)"));
      }
      resetMidiReceiver();
      break;
  }
}

}

void serialModesReset() {
  resetMidiReceiver();
  liveCommandLength = 0;
  liveCommandOverflow = false;
}

void serialModeLiveUpdate() {
  while (Serial.available() > 0) {
    receiveLiveByte(static_cast<char>(Serial.read()));
  }
}

void serialModeRecordUpdate() {
  if (midiState != MIDI_WAIT_HEADER &&
      static_cast<uint32_t>(millis() - midiLastByteAtMs) > 2000) {
    Serial.println(F("ERRO: timeout da trama MIDI"));
    resetMidiReceiver();
  }
  while (Serial.available() > 0) {
    receiveMidiByte(static_cast<uint8_t>(Serial.read()));
  }
}
