#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

static const uint8_t BUZZER_COUNT = 3;
static const uint8_t BUZZER_PINS[BUZZER_COUNT] = {8, 9, 10};
static const uint8_t BUTTON_PINS[BUZZER_COUNT] = {2, 3, 4};
static const uint8_t LED_PINS[BUZZER_COUNT] = {5, 6, 7};

enum SystemMode : uint8_t {
  MODE_PRESETS = 1,
  MODE_LIVE = 2,
  MODE_RECORD = 3
};

static const uint8_t MAX_EVENTS = 80;
static const uint8_t EVENT_SIZE = 3;
static const uint8_t EEPROM_MAGIC = 0x5A;
static const uint16_t SLOT_SIZE = 2 + MAX_EVENTS * EVENT_SIZE;
static const uint8_t MIDI_HEADER = 0xAA;
static const uint16_t MAX_FRAME_DATA = MAX_EVENTS * EVENT_SIZE;

static const uint8_t MIDI_MIN_NOTE = 60;
static const uint8_t MIDI_MAX_NOTE = 95;
static const uint8_t MIDI_MIN_DURATION = 5;
static const uint8_t MIDI_MAX_DURATION = 200;
static const uint16_t LIVE_MIN_FREQUENCY = 100;
static const uint16_t LIVE_MAX_FREQUENCY = 1976;

#endif
