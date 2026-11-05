#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "buzzer.h"
#include "config.h"
#include "serial_modes.h"
#include "uart.h"

typedef enum {
  MIDI_WAIT_HEADER,
  MIDI_WAIT_SLOT,
  MIDI_WAIT_LENGTH,
  MIDI_WAIT_EVENTS,
  MIDI_WAIT_CHECKSUM
} MidiReceiveState;

static uint8_t midi_data[MAX_FRAME_DATA];
static MidiReceiveState midi_state = MIDI_WAIT_HEADER;
static uint8_t midi_slot;
static uint8_t midi_length;
static uint8_t midi_data_index;
static uint8_t midi_checksum;
static uint8_t midi_frame_valid = 1;
static uint32_t midi_last_byte_at_ms;

static char live_command[32];
static uint8_t live_command_length;
static uint8_t live_command_overflow;

static uint8_t parse_unsigned(const char *text, uint8_t length, uint16_t *value) {
  uint8_t i;
  uint32_t parsed = 0;

  if (length == 0U) {
    return 0;
  }
  for (i = 0; i < length; ++i) {
    if (text[i] < '0' || text[i] > '9') {
      return 0;
    }
    parsed = parsed * 10U + (uint8_t)(text[i] - '0');
    if (parsed > 65535U) {
      return 0;
    }
  }
  *value = (uint16_t)parsed;
  return 1;
}

static uint8_t parse_live_command(const char *command, uint8_t length,
                                  uint8_t *channel, uint16_t *frequency,
                                  uint16_t *duration) {
  uint8_t i;
  uint8_t first_separator = 0;
  uint8_t second_separator = 0;
  uint16_t buzzer_number;

  if (length < 7U || command[0] != 'B') {
    return 0;
  }
  for (i = 1; i < length; ++i) {
    if (command[i] == '_') {
      if (first_separator == 0U) {
        first_separator = i;
      } else if (second_separator == 0U) {
        second_separator = i;
      } else {
        return 0;
      }
    }
  }
  if (first_separator == 0U || second_separator == 0U ||
      second_separator <= first_separator + 1U || second_separator + 1U >= length) {
    return 0;
  }

  if (!parse_unsigned(command + 1, first_separator - 1U, &buzzer_number) ||
      !parse_unsigned(command + first_separator + 1U,
                      second_separator - first_separator - 1U, frequency) ||
      !parse_unsigned(command + second_separator + 1U,
                      length - second_separator - 1U, duration) ||
      buzzer_number < 1U || buzzer_number > BUZZER_COUNT ||
      (*duration != 0U &&
       (*frequency < LIVE_MIN_FREQUENCY || *frequency > LIVE_MAX_FREQUENCY)) ||
      (*duration != 0U && (*duration < 50U || *duration > 2000U))) {
    return 0;
  }

  *channel = (uint8_t)buzzer_number;
  return 1;
}

static void process_live_command(void) {
  uint8_t channel;
  uint16_t frequency;
  uint16_t duration;

  if (live_command_overflow ||
      !parse_live_command(live_command, live_command_length, &channel,
                          &frequency, &duration)) {
    uart_write_string_P(PSTR("ERRO: comando invalido\r\n"));
  } else if (duration == 0U) {
    buzzer_stop(channel - 1U);
    uart_write_string_P(PSTR("OK: pausa\r\n"));
  } else {
    buzzer_start(channel - 1U, frequency, duration);
    uart_write_string_P(PSTR("OK\r\n"));
  }

  live_command_length = 0;
  live_command_overflow = 0;
}

static void receive_live_byte(char value) {
  if (value == ' ' || value == '\t' || value == '\r' || value == '\n') {
    if (live_command_length > 0U || live_command_overflow) {
      process_live_command();
    }
    return;
  }

  if (live_command_length < sizeof(live_command) - 1U) {
    live_command[live_command_length++] = value;
  } else {
    live_command_overflow = 1;
  }
}

static void write_preset_to_eeprom(void) {
  const uint16_t address = (uint16_t)(midi_slot - 1U) * SLOT_SIZE;
  uint16_t i;

  eeprom_update_byte((uint8_t *)(uintptr_t)address, 0);
  for (i = 0; i < midi_length; ++i) {
    eeprom_update_byte((uint8_t *)(uintptr_t)(address + 2U + i), midi_data[i]);
  }
  eeprom_update_byte((uint8_t *)(uintptr_t)(address + 1U),
                     midi_length / EVENT_SIZE);
  eeprom_update_byte((uint8_t *)(uintptr_t)address, EEPROM_MAGIC);

  uart_write_string_P(PSTR("OK: musica "));
  uart_write_u16(midi_slot);
  uart_write_string_P(PSTR(" gravada ("));
  uart_write_u16(midi_length / EVENT_SIZE);
  uart_write_string_P(PSTR(" eventos)\r\n"));
}

static void reset_midi_receiver(void) {
  midi_state = MIDI_WAIT_HEADER;
  midi_slot = 0;
  midi_length = 0;
  midi_data_index = 0;
  midi_checksum = 0;
  midi_frame_valid = 1;
}

static void receive_midi_byte(uint8_t value) {
  midi_last_byte_at_ms = buzzer_millis();

  switch (midi_state) {
    case MIDI_WAIT_HEADER:
      if (value == MIDI_HEADER) {
        midi_checksum = value;
        midi_frame_valid = 1;
        midi_state = MIDI_WAIT_SLOT;
      }
      break;

    case MIDI_WAIT_SLOT:
      midi_slot = value;
      midi_checksum ^= value;
      if (value < 1U || value > 2U) {
        midi_frame_valid = 0;
      }
      midi_state = MIDI_WAIT_LENGTH;
      break;

    case MIDI_WAIT_LENGTH:
      midi_length = value;
      midi_checksum ^= value;
      if (value < EVENT_SIZE || value > MAX_FRAME_DATA ||
          value % EVENT_SIZE != 0U) {
        uart_write_string_P(PSTR("ERRO: comprimento de trama invalido\r\n"));
        reset_midi_receiver();
      } else {
        midi_data_index = 0;
        midi_state = MIDI_WAIT_EVENTS;
      }
      break;

    case MIDI_WAIT_EVENTS:
      midi_data[midi_data_index++] = value;
      midi_checksum ^= value;
      if ((midi_data_index % EVENT_SIZE) == 1U &&
          (value < 1U || value > BUZZER_COUNT)) {
        midi_frame_valid = 0;
      } else if ((midi_data_index % EVENT_SIZE) == 2U &&
                 value != 0U && (value < MIDI_MIN_NOTE || value > MIDI_MAX_NOTE)) {
        midi_frame_valid = 0;
      } else if ((midi_data_index % EVENT_SIZE) == 0U &&
                 (value < MIDI_MIN_DURATION || value > MIDI_MAX_DURATION)) {
        midi_frame_valid = 0;
      }
      if (midi_data_index == midi_length) {
        midi_state = MIDI_WAIT_CHECKSUM;
      }
      break;

    case MIDI_WAIT_CHECKSUM:
      if (value == midi_checksum && midi_frame_valid) {
        write_preset_to_eeprom();
      } else {
        uart_write_string_P(PSTR("ERRO: trama MIDI invalida\r\n"));
      }
      reset_midi_receiver();
      break;
  }
}

void serial_modes_reset(void) {
  reset_midi_receiver();
  live_command_length = 0;
  live_command_overflow = 0;
}

void serial_mode_live_update(void) {
  uint8_t value;
  while (uart_read(&value)) {
    receive_live_byte((char)value);
  }
}

void serial_mode_record_update(void) {
  uint8_t value;
  if (midi_state != MIDI_WAIT_HEADER &&
      (uint32_t)(buzzer_millis() - midi_last_byte_at_ms) > 2000U) {
    uart_write_string_P(PSTR("ERRO: timeout da trama MIDI\r\n"));
    reset_midi_receiver();
  }
  while (uart_read(&value)) {
    receive_midi_byte(value);
  }
}
