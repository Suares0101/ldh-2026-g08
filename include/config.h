#ifndef CONFIG_H
#define CONFIG_H

#include <avr/io.h>
#include <stdint.h>

#define BUZZER_COUNT 3U
#define MAX_SLOTS 2U                    // músicas guardadas na EEPROM (B1 e B2)
#define MAX_EVENTS 120U                 // eventos por música
#define EVENT_SIZE 3U                   // na trama: canal, nota, duração
#define EEPROM_MAGIC 0xA6U              // marca um slot com gravação completa
#define EEPROM_EVENT_SIZE 2U            // na EEPROM: canal+nota juntos, duração
#define SLOT_SIZE (2U + MAX_EVENTS * EEPROM_EVENT_SIZE) // magic + contagem + eventos
#define MIDI_HEADER 0xAAU               // primeiro byte da trama do modo 3
#define MAX_FRAME_DATA (MAX_EVENTS * EVENT_SIZE)

// Limites aceites (iguais aos do midi_para_trama.pyw); duração em unidades de 10 ms.
#define MIDI_MIN_NOTE 60U
#define MIDI_MAX_NOTE 95U
#define MIDI_MIN_DURATION 5U
#define MIDI_MAX_DURATION 200U
#define LIVE_MIN_FREQUENCY 100U
#define LIVE_MAX_FREQUENCY 2400U

typedef enum {
  MODE_PRESETS = 1,
  MODE_LIVE = 2,
  MODE_RECORD = 3
} SystemMode;

// Pinos: buzzers em PB0-PB2, botões em PD2-PD4, LEDs em PD5-PD7.
static const uint8_t BUZZER_BITS[BUZZER_COUNT] = {_BV(0), _BV(1), _BV(2)};
static const uint8_t BUTTON_BITS[BUZZER_COUNT] = {_BV(2), _BV(3), _BV(4)};
static const uint8_t LED_BITS[BUZZER_COUNT] = {_BV(5), _BV(6), _BV(7)};

#endif
