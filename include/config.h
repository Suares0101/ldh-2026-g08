#ifndef CONFIG_H
#define CONFIG_H

#include <avr/io.h>
#include <stdint.h>

#define BUZZER_COUNT 3U
#define ACTIVE_BUZZER_INDEX 2U
#define BUZZER_RESONANT_FREQUENCY 2400U
#define MAX_EVENTS 80U
#define EVENT_SIZE 3U
#define EEPROM_MAGIC 0x5AU
#define SLOT_SIZE (2U + MAX_EVENTS * EVENT_SIZE)
#define MIDI_HEADER 0xAAU
#define MAX_FRAME_DATA (MAX_EVENTS * EVENT_SIZE)

#define MIDI_MIN_NOTE 60U
#define MIDI_MAX_NOTE 95U
#define MIDI_MIN_DURATION 5U
#define MIDI_MAX_DURATION 200U
#define LIVE_MIN_FREQUENCY 100U
#define LIVE_MAX_FREQUENCY BUZZER_RESONANT_FREQUENCY

typedef enum {
  MODE_PRESETS = 1,
  MODE_LIVE = 2,
  MODE_RECORD = 3
} SystemMode;

static const uint8_t BUZZER_BITS[BUZZER_COUNT] = {_BV(0), _BV(1), _BV(2)};
static const uint8_t BUTTON_BITS[BUZZER_COUNT] = {_BV(2), _BV(3), _BV(4)};
static const uint8_t LED_BITS[BUZZER_COUNT] = {_BV(5), _BV(6), _BV(7)};

#endif
