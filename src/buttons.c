#include <avr/io.h>
#include <avr/pgmspace.h>
#include "uart.h"

// USART0 a 9600 baud, 8N1, só com polling (sem interrupções).
void uart_init(void) {
  const uint16_t divisor = (uint16_t)(F_CPU / (16UL * 9600UL) - 1UL);
  UCSR0A = 0;
  UBRR0H = (uint8_t)(divisor >> 8);
  UBRR0L = (uint8_t)divisor;
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

// Não bloqueia: devolve false se ainda não chegou nenhum byte.
bool uart_read(uint8_t *value) {
  if ((UCSR0A & _BV(RXC0)) == 0U) {
    return false;
  }
  *value = UDR0;
  return true;
}

// Bloqueia até o registo de envio estar livre.
void uart_write(uint8_t value) {
  while ((UCSR0A & _BV(UDRE0)) == 0U) {
  }
  UDR0 = value;
}

// Texto guardado na flash (PSTR) para poupar os 2 KB de RAM.
void uart_write_string_P(const char *text) {
  uint8_t value;
  while ((value = pgm_read_byte(text++)) != '\0') {
    uart_write(value);
  }
}

// Escreve o número em decimal sem usar printf (poupa flash).
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
