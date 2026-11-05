#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void buzzer_init(void);
void buzzer_start(uint8_t index, uint16_t frequency, uint16_t duration_ms);
void buzzer_stop(uint8_t index);
void buzzer_stop_all(void);
uint16_t midi_note_frequency(uint8_t note);
uint32_t buzzer_millis(void);

#endif
