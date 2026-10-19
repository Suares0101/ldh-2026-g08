# Estado da Arte

Sistema com ATmega328P que toca música em 3 buzzers (modo preset, modo live por UART e modo de reconfiguração por trama tipo MIDI).

## 1. SparkFun: "How to Play Multiple Buzzers at Once"

- **Fonte:** https://learn.sparkfun.com/tutorials/how-to-play-multiple-buzzers-at-once
- **O que faz:**
  - Liga 3 buzzers Qwiic a uma RedBoard e toca melodia, harmonia e baixo de uma peça (tema do Super Mario Bros).
  - Cada buzzer só toca uma nota de cada vez, por isso a polifonia resulta de usar vários buzzers, cada um com o seu endereço I2C.
- **Componentes:**
  - Placa SparkFun RedBoard.
  - 3 buzzers SparkFun Qwiic (endereços I2C diferentes).
  - Cabos Qwiic e biblioteca Arduino do fabricante.
- **O que podemos reutilizar:**
  - A divisão da música em 3 vozes com papéis diferentes (melodia, harmonia, baixo).
  - A organização das notas em arrays, uma por voz.
- **O que devemos evitar:**
  - I2C e módulos Qwiic: encarecem o projeto e escondem a geração do som, que no nosso caso deve ser feita pelo ATmega328P.
  - Dependência de uma biblioteca de terceiros.
  - Música fixa no código: não tem botões, UART nem EEPROM.

## 2. Hackster: "Happy Birthday" com polifonia a 3 vozes em Arduino Nano

- **Fonte:** https://www.hackster.io/liss/happy-birthday-melody-on-arduino-nano-with-3-way-polyphony-c80c31
- **O que faz:**
  - Toca o "Happy Birthday" com 3 buzzers, numa mini breadboard.
  - Cada buzzer tem um papel (solo, baixo e ritmo) e há LEDs que piscam a cada mudança de nota no canal correspondente.
- **Componentes:**
  - Arduino Nano (mesmo microcontrolador que o nosso, ATmega328P).
  - 3 buzzers passivos.
  - LEDs de estado.
  - Biblioteca Tone (versão antiga).
- **O que podemos reutilizar:**
  - O mesmo microcontrolador e o mesmo número de buzzers.
  - A atribuição de uma função a cada buzzer.
  - LEDs de estado, com resistência em série (ex.: 220 Ω a 330 Ω), para indicar o modo ativo ou a nota a tocar.
- **O que devemos evitar:**
  - Usar pinos de I/O em LOW como se fossem GND: ligar os buzzers ao GND verdadeiro.
  - Depender de uma versão antiga da biblioteca Tone, sem garantia de compatibilidade com versões atuais. Preferimos gerar as frequências com os temporizadores do ATmega328P.
  - Música fixa no código: não tem botão de seleção, UART nem EEPROM.

## 3. Referência adicional: fórum Arduino, "16 Individual Buzzers"

- **Fonte:** https://forum.arduino.cc/t/16-individual-buzzers/657264
- **O que faz:** usa 16 buzzers para formar uma voz polifónica. As notas são lidas de ficheiros com linhas do tipo (tempo, canal, nota ou mensagem), guardadas num array e tocadas na altura certa.
- **Componentes:** Arduino MEGA 2560, cartão micro SD, 16 buzzers.
- **O que podemos reutilizar:** a ideia de descrever cada nota por tempo, canal e nota, que serve de base para a trama tipo MIDI do modo de reconfiguração.
- **O que devemos evitar:** cartão SD e MEGA 2560, que estão fora do âmbito do projeto (o nosso armazenamento é a EEPROM).

## Comparação com o nosso projeto

- **Hardware:** ATmega328P, 3 buzzers, alimentação por USB (como o projeto 2, sem módulos I2C).
- **Seleção de músicas:** botão e EEPROM (nenhum dos exemplos tem).
- **Modo live:** notas e durações enviadas por UART a 9600 baud (nenhum dos exemplos tem).
- **Reconfiguração:** trama tipo MIDI, validada e gravada na EEPROM (só o fórum tem uma ideia de formato, e a partir de SD).

## Conclusões

- O projeto 2 é a melhor referência de hardware: mesmo microcontrolador, 3 buzzers, montagem simples e barata.
- O projeto 1 serve como inspiração para organizar as 3 vozes.
- O fórum serve como inspiração para o formato (tempo, canal, nota) da trama do modo 3.
- Nenhum dos exemplos tem seleção por botão, UART ou gravação em EEPROM, e é aí que o nosso projeto se distingue.

## Questões em aberto (a verificar)

- O ATmega328P tem 3 temporizadores, e um deles (Timer0) é usado pelo Arduino para `millis()` e `delay()`. Decidir se os tons são gerados por temporizadores ou por software.
- A EEPROM do ATmega328P tem 1 KB: definir um formato compacto para a trama (por exemplo, nota e duração em 2 bytes) para caberem vários presets.
