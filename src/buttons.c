#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include "buttons.h"
#include "config.h"

static uint8_t pressed_state;

void buttons_init(void) {
  DDRD &= (uint8_t)~(_BV(PD2) | _BV(PD3) | _BV(PD4));
  PORTD |= _BV(PD2) | _BV(PD3) | _BV(PD4);
  pressed_state = 0U;
}

bool button_pressed(uint8_t index) {
  uint8_t mask;
  bool pressed;

  if (index >= BUZZER_COUNT) {
    return false;
  }

  mask = BUTTON_BITS[index];
  if ((PIND & mask) != 0U) {
    pressed_state &= (uint8_t)~(1U << index);
    return false;
  }

  pressed = (pressed_state & (uint8_t)(1U << index)) == 0U;
  if (pressed) {
    pressed_state |= (uint8_t)(1U << index);
  }
  return pressed;
}
