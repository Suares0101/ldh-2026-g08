#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void buzzerInit();
void buzzerStart(uint8_t index, uint16_t frequency, uint16_t durationMs);
void buzzerStop(uint8_t index);
void buzzerStopAll();
void buzzerUpdate();
uint16_t midiNoteFrequency(uint8_t note);

#endif
