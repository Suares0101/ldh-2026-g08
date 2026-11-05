#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <stdint.h>

void uart_init(void);
bool uart_read(uint8_t *value);
void uart_write(uint8_t value);
void uart_write_string(const char *text);
void uart_write_string_P(const char *text);
void uart_write_u16(uint16_t value);
void uart_write_line(const char *text);

#endif
