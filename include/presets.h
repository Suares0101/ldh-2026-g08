#ifndef PRESETS_H
#define PRESETS_H

#include <stdint.h>

void preset_stop_playback(void);
void preset_play(uint8_t slot);
void preset_update(void);

#endif
