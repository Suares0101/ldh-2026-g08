# Casos de Uso e Diagrama de Blocos (preliminar)

**Projeto:** Sistema musical com 3 buzzers (ATmega328P)

## Atores

- **Utilizador final:** escolhe e ouve músicas através dos botões.
- **Utilizador técnico:** liga o sistema ao PC por USB e usa um terminal série para tocar notas ou enviar músicas novas.

## Diagrama de casos de uso

```mermaid
flowchart LR
  FINAL["Utilizador final<br/>botões"]
  TEC["Utilizador técnico<br/>PC + terminal"]

  subgraph SISTEMA["Sistema ATmega328P"]
    UC1(["UC1 Presets<br/>Tocar música 1 ou 2 (B1/B2)"])
    UC2(["UC2 Live<br/>Tocar notas em tempo real"])
    UC3(["UC3 Reconfiguração<br/>Gravar nova música"])

    LER(["Ler preset da EEPROM"])
    RECEBER(["Receber nota e duração por UART"])
    VALIDAR(["Validar trama"])
    GRAVAR(["Gravar na EEPROM"])
  end

  FINAL --- UC1
  TEC --- UC2
  TEC --- UC3

  UC1 -.->|include| LER
  UC2 -.->|include| RECEBER
  UC3 -.->|include| VALIDAR
  UC3 -.->|include| GRAVAR

  classDef ator fill:#ffffff,stroke:#111,stroke-width:2px
  classDef uc fill:#E8F0FE,stroke:#1A4FA0,stroke-width:2px
  classDef inc fill:#ffffff,stroke:#555
  class FINAL,TEC ator
  class UC1,UC2,UC3 uc
  class LER,RECEBER,VALIDAR,GRAVAR inc
```

### UC1: Presets (botões B1 e B2)

- **Ator:** utilizador final.
- **Pré-condição:** sistema alimentado por USB e com pelo menos um preset na EEPROM.
- **Fluxo principal:**
  - Com o modo preset ativo (LED1 aceso), o utilizador carrega em B1 (música 1) ou B2 (música 2).
  - O sistema lê o preset da EEPROM.
  - O sistema toca a música nos 3 buzzers, uma voz por buzzer.
  - O LED do modo preset acende.
- **Fluxos alternativos:**
  - Se a EEPROM não tiver presets válidos, o sistema não toca e indica o erro.
  - Se o utilizador carregar em B3, o som é interrompido e o sistema muda de modo.
- **Pós-condição:** os presets mantêm-se guardados depois de desligar.
- **Requisitos associados:** RF01 a RF04, RF12 a RF14.

### UC2: Live

- **Ator:** utilizador técnico.
- **Pré-condição:** PC ligado por USB e terminal aberto a 9600 baud.
- **Fluxo principal:**
  - O utilizador carrega em B3 até chegar ao modo live.
  - O LED do modo live acende.
  - O PC envia o comando `B<buzzer>_<Hz>_<ms>` por UART (ex.: `B1_300_250`).
  - O sistema recebe os dados e toca a nota em tempo real.
- **Fluxos alternativos:**
  - Se a nota estiver fora da gama ou a duração for inválida, o sistema ignora o pedido e responde com erro.
  - Se o utilizador carregar em B3, o som é interrompido e o sistema muda de modo.
- **Pós-condição:** nada é gravado na EEPROM.
- **Requisitos associados:** RF01, RF05 a RF07, RF12 a RF14.

### UC3: Reconfiguração

- **Ator:** utilizador técnico.
- **Pré-condição:** PC ligado por USB e terminal aberto a 9600 baud.
- **Fluxo principal:**
  - O utilizador carrega em B3 até chegar ao modo de reconfiguração.
  - O LED do modo de reconfiguração acende.
  - O PC envia uma trama tipo MIDI por UART.
  - O sistema valida a trama (cabeçalho, comprimento e checksum).
  - O sistema grava a música numa das 2 posições de preset da EEPROM e responde "OK".
  - A música fica disponível como novo preset em UC1.
- **Fluxos alternativos:**
  - Se a trama for inválida, o sistema responde "ERRO" e não altera a EEPROM.
  - Se a música não couber na EEPROM, o sistema responde com erro e mantém as músicas existentes.
- **Pós-condição:** os novos presets ficam guardados depois de desligar.
- **Requisitos associados:** RF08 a RF12, RF14.

## Diagrama de blocos

As setas a tracejado vermelho são a alimentação de 5 V vinda do USB. O GND é comum a todos os blocos e não está desenhado.

```mermaid
flowchart LR
  PC["PC<br/>Terminal série 9600 baud"]
  USB["USB<br/>Cabo e conector"]
  PONTE["Ponte USB-UART<br/>ex.: CH340, FT232"]
  MCU["ATmega328P<br/>UART · Temporizadores<br/>EEPROM 1 KB · Firmware 3 modos"]
  DRV["Drivers<br/>3 × transístor + resistência"]
  BZ["Buzzers<br/>BZ1 · BZ2 · BZ3"]
  BTN["Botões<br/>B1 · B2 · B3"]
  LED["LEDs de modo<br/>3 × LED + resistência"]

  PC <-->|"dados"| USB
  USB <-->|"D+ / D-"| PONTE
  PONTE <-->|"TX / RX"| MCU
  MCU -->|"3 sinais"| DRV
  DRV -->|"3 saídas"| BZ
  BTN -->|"3 entradas"| MCU
  MCU -->|"3 saídas"| LED

  USB -.->|"+5 V"| PONTE
  USB -.->|"+5 V"| MCU
  USB -.->|"+5 V"| DRV
  USB -.->|"+5 V"| BZ

  classDef bloco fill:#E8F0FE,stroke:#1A4FA0,stroke-width:2px
  classDef cpu fill:#FFF3D6,stroke:#B26A00,stroke-width:2px
  class PC,USB,PONTE,DRV,BZ,BTN,LED bloco
  class MCU cpu
  linkStyle 7,8,9,10 stroke:#C62828,stroke-width:2px
```

### Blocos

- **PC:** envia notas e tramas por terminal série (9600 baud).
- **USB:** cabo e conector; leva dados ao PC e fornece os 5 V de alimentação.
- **Ponte USB-UART:** converte os sinais USB em UART (TX/RX). Exemplos de integrados: CH340 ou FT232 (a escolher).
- **ATmega328P:**
  - UART para comunicar com o PC.
  - Temporizadores para gerar os 3 tons.
  - EEPROM de 1 KB para guardar os presets.
  - Firmware com os 3 modos.
- **Drivers:** 3 circuitos (transístor + resistência) que alimentam os buzzers a partir dos sinais do microcontrolador.
- **Buzzers:** 3 buzzers, um por voz.
- **Botões:** B1 e B2 tocam as músicas 1 e 2 no modo preset; B3 muda de modo (3 entradas).
- **LEDs de modo:** 3 LEDs com resistência em série, um por modo (3 saídas).
- **Alimentação:** 5 V vindos do USB para a ponte, o microcontrolador, os drivers e os buzzers, com GND comum.

## Pontos a decidir antes da aula 03

- Ponte USB-UART: integrado dedicado ou módulo pronto.
- Tipo de buzzer (passivo, para gerar frequências diferentes) e corrente que consome, para dimensionar os drivers.
- Se o consumo total fica dentro de RNF01 (≤ 150 mA).
- Atribuição de pinos do ATmega328P (UART, 3 saídas de tom, 3 botões, 3 LEDs).
- Formato da trama do modo 3 (cabeçalho, comprimento, dados, checksum), com 2 posições de preset.
