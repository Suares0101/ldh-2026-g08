# Sistema Musical com 3 Buzzers

Firmware em C para o ATmega328P do Arduino Uno (16 MHz), com três modos:
reprodução de dois presets da EEPROM, reprodução de notas enviadas por UART e
gravação de novos presets a partir de ficheiros MIDI. Os três buzzers podem
reproduzir vozes independentes em simultâneo.
 
## Índice

- [Objetivo](#objetivo)
- [Modos de funcionamento](#modos-de-funcionamento)
- [Hardware](#hardware)
- [Comunicação UART](#comunicação-uart)
- [Formato da trama de gravação](#formato-da-trama-de-gravação)
- [Implementação](#implementação)
- [Fluxograma](#fluxograma)
- [Limites atuais](#limites-atuais)
- [Estrutura do repositório](#estrutura-do-repositório)
- [Compilar e carregar](#compilar-e-carregar)
- [Prazos](#prazos)

## Objetivo

Sistema embebido que reproduz música em 3 buzzers (uma voz por buzzer),
alimentado por USB e controlado por botões e por um terminal série no PC.

## Modos de funcionamento

O botão B3 percorre os modos pela ordem 1 → 2 → 3 → 1. Um LED indica o modo
ativo (LED1 = modo 1, LED2 = modo 2, LED3 = modo 3). Mudar de modo interrompe
a reprodução em curso e descarta comandos ou tramas incompletos.

| Modo | Descrição |
| --- | --- |
| 1 – Presets | B1 toca o preset 1 e B2 toca o preset 2, guardados na EEPROM. Cada buzzer reproduz a sua própria voz em simultâneo. |
| 2 – Live | O PC envia um ou mais comandos `B<buzzer>_<Hz>_<ms>` por UART. Cada comando toca no buzzer indicado sem guardar a nota; duração `0` pára esse buzzer. |
| 3 – Gravação | O PC envia a trama binária gerada pelo conversor MIDI. A placa valida os eventos e o checksum e grava o slot 1 ou 2 na EEPROM. |

## Hardware

Pinos usados no teste atual (placa Arduino com a mesma ligação do Tinkercad):

| Função | Pino Uno | Pino AVR |
| --- | --- | --- |
| Buzzer 1 | D8 | PB0 |
| Buzzer 2 | D9 | PB1 |
| Buzzer 3 | D10 | PB2 |
| Botão 1 (música 1 no modo preset) | D2 | PD2 |
| Botão 2 (música 2 no modo preset) | D3 | PD3 |
| Botão 3 (muda de modo) | D4 | PD4 |
| LED 1 (modo 1) | D5 | PD5 |
| LED 2 (modo 2) | D6 | PD6 |
| LED 3 (modo 3) | D7 | PD7 |
| UART RX/TX | D0/D1 | PD0/PD1 |

- Microcontrolador: ATmega328P a 16 MHz.
- Alimentação: USB (5 V).
- Botões ligados a GND quando premidos; as entradas usam resistências pull-up
  internas.
- Os buzzers são passivos. Timer2 gera interrupções periódicas para atualizar
  individualmente os três sinais nos pinos D8–D10.
- Os três LEDs indicam exclusivamente o modo ativo.
- Placa final: PCB própria de 2 camadas, ≤ 80 × 60 mm, desenhada em Altium 365,
  com ponte USB-UART (por exemplo CH340) e conector ISP.
- Simulação do circuito em Tinkercad.
- Usar buzzers passivos para reproduzir diferentes frequências.
- Verificar a corrente exigida por cada buzzer e usar um transistor como driver
  quando necessário; não exceder o limite de corrente dos GPIO.

## Comunicação UART

Configuração: **9600 baud, 8 bits de dados, sem paridade, 1 stop bit (8-N-1)**.
O Monitor Série deve usar 9600 baud.

### Modo live

Formato do comando:

```text
B<buzzer>_<frequência em Hz>_<duração em ms>
```

Exemplo (dois comandos enviados na mesma linha):

```text
B2_200_500 B3_100_200
```

- Buzzer: `1` a `3`; frequência: 100 Hz a 1976 Hz; duração: 50 ms a 2000 ms.
- Separar comandos com espaços, tabulações ou Enter (LF/CRLF). Cada comando é
  processado assim que chega o separador, sem esperar pelo fim da linha.
- Exemplo: `B2_200_500 B3_100_200` toca nos buzzers 2 e 3 com durações
  independentes.
- Uma duração de `0` pára o buzzer indicado e ignora a frequência. Exemplo:
  `B2_100_0`.
- Cada comando válido recebe `OK`; uma pausa recebe `OK: pausa`; comandos
  inválidos recebem `ERRO`.
- O buffer aceita comandos até 31 caracteres; comandos maiores são rejeitados.

## Formato da trama de gravação

No modo 3, enviar os bytes binários produzidos por `src/midi_para_trama.py`.
Os campos são transmitidos por ordem, um byte de cada vez:

| Campo | Tamanho | Descrição |
| --- | --- | --- |
| Cabeçalho | 1 byte | `0xAA` |
| Slot | 1 byte | Música a substituir: `1` ou `2` |
| Comprimento | 1 byte | Número de bytes dos eventos: 3 a 240, múltiplo de 3 |
| Eventos | 3 bytes cada | Buzzer/canal (1–3), nota MIDI (0 ou 60–95), duração em unidades de 10 ms (5–200) |
| Checksum | 1 byte | XOR de todos os bytes anteriores |

- Cada preset aceita de 1 a 80 eventos. A nota MIDI `0` é uma pausa no buzzer
  desse evento; as outras notas aceites são 60–95 (C4–B6).
- A EEPROM só é atualizada depois de validar slot, comprimento, eventos e
  checksum. O slot fica inválido durante a escrita e só é marcado válido no
  fim.
- A placa responde `OK: musica <slot> gravada (<eventos> eventos)` ou `ERRO`.
- Exemplo no Windows: `python src\midi_para_trama.py musica.mid --slot 1 --porta COM3`.
- O conversor necessita de `mido` e `pyserial` (`pip install mido pyserial`).
  Ao abrir a porta série, o Uno reinicia; carregar em B3 até selecionar o modo
  3 quando o script pedir. Fechar o Monitor Série antes de enviar.

## Implementação

- Firmware em C (`.c`): `main`, `buttons`, `buzzer`, `presets`,
  `serial_modes` e `uart`. A inicialização e o ciclo principal implementam
  `setup()` e `loop()` em C para o core Arduino.
- `include/` – cabeçalhos com configuração e interfaces dos módulos.
- GPIO, Timer2, UART e EEPROM são controlados diretamente pelos registos AVR
  e pela avr-libc. A aplicação não usa as APIs C++ `Serial` ou `EEPROM`; o
  framework Arduino fornece o arranque da placa.
- Timer2 usa CTC, prescaler 8 e comparação a cada 20 μs para atualizar os
  sinais dos três buzzers e fornecer a base de tempo do firmware.
- `src/midi_para_trama.py` – conversor de ficheiros MIDI para a trama binária
  recebida no modo 3.

## Fluxograma

```mermaid
flowchart TD
  START([Reset]) --> INIT[Inicializar buzzers, botões, LEDs, UART e EEPROM]
  INIT --> M1

  M1[Modo 1 - Preset<br/>LED1 aceso]
  M2[Modo 2 - Live<br/>LED2 aceso]
  M3[Modo 3 - Gravação<br/>LED3 aceso]

  M1 -- B3 --> M2
  M2 -- B3 --> M3
  M3 -- B3 --> M1

  M1 -- B1 --> P1[Ler música 1 da EEPROM e tocar nos 3 buzzers]
  M1 -- B2 --> P2[Ler música 2 da EEPROM e tocar nos 3 buzzers]
  P1 --> M1
  P2 --> M1

  M2 --> CMD[Receber comando(s) UART]
  CMD --> PARSE_LIVE{Comando válido?}
  PARSE_LIVE -- Sim --> TONE[Tocar ou parar buzzer indicado]
  PARSE_LIVE -- Não --> ERR_LIVE[Enviar erro]
  TONE --> M2
  ERR_LIVE --> M2

  M3 --> RECV[Receber trama MIDI por UART]
  RECV --> VALID{Trama válida?}
  VALID -- Sim --> WRITE[Gravar numa das 2 posições da EEPROM]
  VALID -- Não --> ERR_MIDI[Enviar erro]
  WRITE --> M3
  ERR_MIDI --> M3
```

## Limites atuais

- 2 músicas guardadas na EEPROM (presets 1 e 2); cada slot ocupa 242 bytes
  (2 bytes de metadados e até 240 bytes de eventos), totalizando 484 bytes.
- Máximo de 80 eventos por música.
- Gama de frequências live: 100 Hz a 1976 Hz.
- Timer2 é usado pelo firmware para os três buzzers e deixa de estar disponível
  para outras funções que dependam desse temporizador.
- A frequência/timbre deve ser confirmada com os buzzers e o circuito finais;
  os sinais são ondas quadradas geradas por software numa interrupção de 20 μs.
- A EEPROM do ATmega328P tem 1 KB; os dois slots usam 484 bytes.
- A EEPROM tem um número limitado de ciclos de escrita; evitar regravações
  desnecessárias.

## Estrutura do repositório

```text
.
├── docs/        # requisitos, estado da arte e documentação do projeto
├── include/     # configuração e interfaces C
├── src/         # firmware C e conversor MIDI em Python
├── platformio.ini
└── README.md
```

## Compilar e carregar

Toolchain: PlatformIO, perfil Arduino Uno (`platformio.ini`), ATmega328P a
16 MHz. Na raiz do projeto:

```powershell
pio run -e uno
pio run -e uno --target upload
```

O comando de upload usa a porta predefinida pelo PlatformIO. Para selecionar
uma porta específica, acrescentar `--upload-port COM3` (substituir pela porta
ligada à placa).

## Prazos

- E1: 26/10/2026 (tag `E1` no Git)
- E2: 05/11/2026
- E3: 14/12/2026
- PF: 28/01/2027
