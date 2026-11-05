#include <avr/io.h>
#include "buttons.h"
#include "buzzer.h"
#include "config.h"

typedef struct {
  bool stable_high;
  bool last_reading_high;
  uint32_t changed_at_ms;
} ButtonState;

static ButtonState buttons[BUZZER_COUNT];

void buttons_init(void) {
  uint8_t i;
  const uint32_t now = buzzer_millis();

  for (i = 0; i < BUZZER_COUNT; ++i) {
    const bool reading_high = (PIND & BUTTON_BITS[i]) != 0;
    buttons[i].stable_high = reading_high;
    buttons[i].last_reading_high = reading_high;
    buttons[i].changed_at_ms = now;
  }
}

bool button_pressed(uint8_t index) {
  bool reading_high;
  uint32_t now;

  if (index >= BUZZER_COUNT) {
    return false;
  }

  reading_high = (PIND & BUTTON_BITS[index]) != 0;
  now = buzzer_millis();
  if (reading_high != buttons[index].last_reading_high) {
    buttons[index].last_reading_high = reading_high;
    buttons[index].changed_at_ms = now;
  }

  if ((uint32_t)(now - buttons[index].changed_at_ms) < 30U ||
      reading_high == buttons[index].stable_high) {
    return false;
  }

  buttons[index].stable_high = reading_high;
  return !reading_high;
}
