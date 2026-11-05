#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

void buttons_init(void);
bool button_pressed(uint8_t index);

#endif
