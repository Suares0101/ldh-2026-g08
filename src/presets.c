#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "buzzer.h"
#include "config.h"
#include "presets.h"
#include "uart.h"

typedef struct {
  uint8_t scan_index;
  uint8_t waiting;
  uint8_t finished;
  uint32_t event_started_at_ms;
  uint16_t event_duration_ms;
} VoiceState;

static VoiceState voices[BUZZER_COUNT];
static uint8_t playing;
static uint8_t playing_slot;
static uint8_t event_count;

static uint16_t slot_address(uint8_t slot) {
  return (uint16_t)(slot - 1U) * SLOT_SIZE;
}

static uint8_t preset_valid(uint8_t slot, uint8_t *count) {
  uint16_t address;

  if (slot < 1U || slot > MAX_SLOTS) {
    return 0;
  }
  address = slot_address(slot);
  if (eeprom_read_byte((const uint8_t *)(uintptr_t)address) != EEPROM_MAGIC) {
    return 0;
  }

  *count = eeprom_read_byte((const uint8_t *)(uintptr_t)(address + 1U));
  return *count > 0U && *count <= MAX_EVENTS;
}

static uint8_t read_event(uint8_t index, uint8_t *channel, uint8_t *note,
                          uint8_t *duration) {
  uint8_t packed_note_channel;
  uint16_t address;

  if (index >= event_count) {
    return 0;
  }
  address = slot_address(playing_slot) + 2U +
            (uint16_t)index * EEPROM_EVENT_SIZE;
  packed_note_channel =
      eeprom_read_byte((const uint8_t *)(uintptr_t)address);
  *channel = (uint8_t)(packed_note_channel / 37U) + 1U;
  packed_note_channel %= 37U;
  *note = packed_note_channel == 0U
              ? 0U
              : (uint8_t)(packed_note_channel + MIDI_MIN_NOTE - 1U);
  *duration = eeprom_read_byte((const uint8_t *)(uintptr_t)(address + 1U));

  return *channel >= 1U && *channel <= BUZZER_COUNT &&
         (*note == 0U || (*note >= MIDI_MIN_NOTE && *note <= MIDI_MAX_NOTE)) &&
         *duration >= MIDI_MIN_DURATION && *duration <= MIDI_MAX_DURATION;
}

static void schedule_next_event(uint8_t voice_index, uint32_t now_ms) {
  VoiceState *voice = &voices[voice_index];

  while (voice->scan_index < event_count) {
    uint8_t channel;
    uint8_t note;
    uint8_t duration;
    const uint8_t index = voice->scan_index++;

    if (!read_event(index, &channel, &note, &duration)) {
      voice->finished = 1;
      buzzer_stop(voice_index);
      return;
    }
    if (channel != voice_index + 1U) {
      continue;
    }

    voice->waiting = 1;
    voice->event_started_at_ms = now_ms;
    voice->event_duration_ms = (uint16_t)duration * 10U;
    buzzer_start(voice_index, midi_note_frequency(note), voice->event_duration_ms);
    return;
  }

  voice->finished = 1;
  voice->waiting = 0;
  buzzer_stop(voice_index);
}

void preset_stop_playback(void) {
  playing = 0;
}

void preset_play(uint8_t slot) {
  uint8_t count;
  uint8_t i;

  playing = 0;
  buzzer_stop_all();
  if (!preset_valid(slot, &count)) {
    uart_write_string_P(PSTR("ERRO: musica vazia ou invalida\r\n"));
    return;
  }

  playing_slot = slot;
  event_count = count;
  playing = 1;
  for (i = 0; i < BUZZER_COUNT; ++i) {
    voices[i].scan_index = 0;
    voices[i].waiting = 0;
    voices[i].finished = 0;
    schedule_next_event(i, buzzer_millis());
  }

  uart_write_string_P(PSTR("A tocar musica "));
  uart_write_u16(slot);
  uart_write_string_P(PSTR("\r\n"));
}

void preset_update(void) {
  uint8_t i;
  uint8_t all_finished = 1;
  const uint32_t now = buzzer_millis();

  if (!playing) {
    return;
  }

  for (i = 0; i < BUZZER_COUNT; ++i) {
    VoiceState *voice = &voices[i];
    if (voice->finished) {
      continue;
    }
    all_finished = 0;

    if (voice->waiting &&
        (uint32_t)(now - voice->event_started_at_ms) >= voice->event_duration_ms) {
      voice->waiting = 0;
      buzzer_stop(i);
    }
    if (!voice->waiting) {
      schedule_next_event(i, now);
    }
  }

  for (i = 0; i < BUZZER_COUNT; ++i) {
    if (!voices[i].finished) {
      all_finished = 0;
    }
  }
  if (all_finished) {
    playing = 0;
  }
}
