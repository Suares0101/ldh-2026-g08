#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>
#include "buzzer.h"
#include "config.h"

#define TIMER_TICK_US 20U
#define TIMER_TICKS_PER_MS (1000U / TIMER_TICK_US)

typedef struct {
  volatile uint8_t active;
  volatile uint8_t output_high;
  volatile uint16_t half_period_us;
  volatile uint16_t phase_us;
  volatile uint32_t remaining_ticks;
} BuzzerState;

static volatile BuzzerState buzzers[BUZZER_COUNT];
static volatile uint32_t elapsed_ms;
static volatile uint8_t ticks_in_ms;

static const uint16_t NOTE_FREQUENCIES[MIDI_MAX_NOTE - MIDI_MIN_NOTE + 1U] = {
  262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494,
  523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988,
  1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976
};

static void buzzer_pin_low(uint8_t index) {
  PORTB &= (uint8_t)~BUZZER_BITS[index];
}

void buzzer_init(void) {
  uint8_t i;

  for (i = 0; i < BUZZER_COUNT; ++i) {
    DDRB |= BUZZER_BITS[i];
    buzzer_pin_low(i);
    buzzers[i].active = 0;
    buzzers[i].output_high = 0;
    buzzers[i].phase_us = 0;
    buzzers[i].remaining_ticks = 0;
  }

  elapsed_ms = 0;
  ticks_in_ms = 0;

  // Timer2 gera uma interrupção a cada 20 us para atualizar os três tons.
  TCCR2A = _BV(WGM21);
  TCCR2B = _BV(CS21);
  OCR2A = 39U;
  TIMSK2 = _BV(OCIE2A);
}

void buzzer_stop(uint8_t index) {
  if (index >= BUZZER_COUNT) {
    return;
  }

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    buzzers[index].active = 0;
    buzzers[index].output_high = 0;
    buzzers[index].remaining_ticks = 0;
    buzzer_pin_low(index);
  }
}

void buzzer_stop_all(void) {
  uint8_t i;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    for (i = 0; i < BUZZER_COUNT; ++i) {
      buzzers[i].active = 0;
      buzzers[i].output_high = 0;
      buzzers[i].remaining_ticks = 0;
      buzzer_pin_low(i);
    }
  }
}

void buzzer_start(uint8_t index, uint16_t frequency, uint16_t duration_ms) {
  if (index >= BUZZER_COUNT) {
    return;
  }
  if (frequency == 0U || duration_ms == 0U) {
    buzzer_stop(index);
    return;
  }

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    buzzers[index].active = 0;
    buzzers[index].output_high = 0;
    buzzers[index].half_period_us = (uint16_t)(500000UL / frequency);
    buzzers[index].phase_us = 0;
    buzzers[index].remaining_ticks =
        (uint32_t)duration_ms * (1000U / TIMER_TICK_US);
    buzzer_pin_low(index);
    buzzers[index].active = 1;
  }
}

uint32_t buzzer_millis(void) {
  uint32_t now;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    now = elapsed_ms;
  }
  return now;
}

uint16_t midi_note_frequency(uint8_t note) {
  if (note < MIDI_MIN_NOTE || note > MIDI_MAX_NOTE) {
    return 0;
  }
  return NOTE_FREQUENCIES[note - MIDI_MIN_NOTE];
}

ISR(TIMER2_COMPA_vect) {
  uint8_t i;

  if (++ticks_in_ms >= TIMER_TICKS_PER_MS) {
    ticks_in_ms = 0;
    ++elapsed_ms;
  }

  for (i = 0; i < BUZZER_COUNT; ++i) {
    if (!buzzers[i].active) {
      continue;
    }

    if (buzzers[i].remaining_ticks <= 1U) {
      buzzers[i].remaining_ticks = 0;
      buzzers[i].active = 0;
      buzzers[i].output_high = 0;
      buzzer_pin_low(i);
      continue;
    }
    --buzzers[i].remaining_ticks;

    buzzers[i].phase_us += TIMER_TICK_US;
    if (buzzers[i].phase_us >= buzzers[i].half_period_us) {
      buzzers[i].phase_us -= buzzers[i].half_period_us;
      buzzers[i].output_high = !buzzers[i].output_high;
      if (buzzers[i].output_high) {
        PORTB |= BUZZER_BITS[i];
      } else {
        buzzer_pin_low(i);
      }
    }
  }
}
