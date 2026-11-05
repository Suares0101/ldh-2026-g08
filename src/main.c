#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "buttons.h"
#include "buzzer.h"
#include "config.h"
#include "presets.h"
#include "serial_modes.h"
#include "uart.h"

static SystemMode current_mode = MODE_PRESETS;

static void set_mode(SystemMode mode) {
  uint8_t i;
  current_mode = mode;
  preset_stop_playback();
  buzzer_stop_all();
  serial_modes_reset();

  PORTD &= (uint8_t)~(_BV(PD5) | _BV(PD6) | _BV(PD7));
  for (i = 0; i < BUZZER_COUNT; ++i) {
    if (i + 1U == (uint8_t)current_mode) {
      PORTD |= LED_BITS[i];
    }
  }

  uart_write_string_P(PSTR("Modo "));
  uart_write_u16((uint8_t)current_mode);
  uart_write_string_P(PSTR("\r\n"));
}

void setup(void) {
  DDRD |= _BV(PD5) | _BV(PD6) | _BV(PD7);
  PORTD &= (uint8_t)~(_BV(PD5) | _BV(PD6) | _BV(PD7));
  DDRD &= (uint8_t)~(_BV(PD2) | _BV(PD3) | _BV(PD4));
  PORTD |= _BV(PD2) | _BV(PD3) | _BV(PD4);

  buzzer_init();
  uart_init();
  buttons_init();
  set_mode(MODE_PRESETS);
  uart_write_string_P(PSTR("Pronto: B3 muda de modo; modo 2 usa B1_300_250.\r\n"));
  sei();
}

void loop(void) {
  if (button_pressed(2U)) {
    const SystemMode next_mode =
        current_mode == MODE_RECORD ? MODE_PRESETS : (SystemMode)(current_mode + 1);
    set_mode(next_mode);
  }

  if (current_mode == MODE_PRESETS) {
    if (button_pressed(0U)) {
      preset_play(1U);
    }
    if (button_pressed(1U)) {
      preset_play(2U);
    }
    preset_update();
  } else if (current_mode == MODE_LIVE) {
    serial_mode_live_update();
  } else {
    serial_mode_record_update();
  }
}
