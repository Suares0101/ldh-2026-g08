#include <avr/io.h>
#include <avr/pgmspace.h>
#include "uart.h"

void uart_init(void) {
  const uint16_t divisor = (uint16_t)(F_CPU / (16UL * 9600UL) - 1UL);
  UCSR0A = 0;
  UBRR0H = (uint8_t)(divisor >> 8);
  UBRR0L = (uint8_t)divisor;
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

bool uart_read(uint8_t *value) {
  if ((UCSR0A & _BV(RXC0)) == 0U) {
    return false;
  }
  *value = UDR0;
  return true;
}

void uart_write(uint8_t value) {
  while ((UCSR0A & _BV(UDRE0)) == 0U) {
  }
  UDR0 = value;
}

void uart_write_string(const char *text) {
  while (*text != '\0') {
    uart_write((uint8_t)*text++);
  }
}

void uart_write_string_P(const char *text) {
  uint8_t value;
  while ((value = pgm_read_byte(text++)) != '\0') {
    uart_write(value);
  }
}

void uart_write_u16(uint16_t value) {
  char digits[5];
  uint8_t length = 0;
  do {
    digits[length++] = (char)('0' + value % 10U);
    value /= 10U;
  } while (value > 0U);
  while (length > 0U) {
    uart_write((uint8_t)digits[--length]);
  }
}

void uart_write_line(const char *text) {
  uart_write_string(text);
  uart_write('\r');
  uart_write('\n');
}
