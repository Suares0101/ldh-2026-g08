# Lista de Requisitos

**Projeto:** Sistema musical com 3 buzzers (ATmega328P)
**Versão:** 0.2 (rascunho)

## Legenda

- **Origem:** `C` = definido pelo cliente; `P` = proposto pelo grupo (valor a validar).
- **Verificação:**
  - **Ensaio:** teste funcional com resultado passa/falha.
  - **Medição:** valor medido com instrumento (multímetro, osciloscópio, cronómetro).
  - **Inspeção:** verificação visual ou documental.
  - **Simulação:** resultado obtido em Tinkercad.
  - **DRC:** Design Rule Check no Altium (0 erros).

## 1. Requisitos funcionais (RF)

| ID | Requisito | Critério | Verificação | Origem |
|----|-----------|----------|-------------|--------|
| RF01 | O sistema toca música em 3 buzzers em simultâneo, uma voz por buzzer. | 3 sinais distintos e simultâneos, um em cada buzzer | Ensaio + medição (osciloscópio) | C |
| RF02 | No modo preset, o botão B1 toca a música 1 e o botão B2 toca a música 2, guardadas na EEPROM. | Som começa ≤ 500 ms após carregar no botão | Medição (cronómetro) | C |
| RF03 | O sistema guarda 2 presets na EEPROM. | 2 presets, cada um com ≥ 64 eventos de nota | Ensaio | P |
| RF04 | Os presets mantêm-se depois de desligar a alimentação. | 10 ciclos de ligar/desligar sem perda de dados | Ensaio | C |
| RF05 | No modo live, o PC envia o buzzer, a frequência e a duração por UART (comando `B<buzzer>_<Hz>_<ms>`, ex.: `B1_300_250`) e o sistema toca em tempo real. | Latência ≤ 50 ms entre o último byte recebido e o início do som | Medição (osciloscópio) | C |
| RF06 | No modo live, o sistema toca notas na gama C4 a B6. | 262 Hz a 1976 Hz, erro de frequência ≤ 2 % | Medição (osciloscópio) | P |
| RF07 | O sistema respeita a duração pedida para cada nota. | 50 ms a 2000 ms, erro ≤ 5 % | Medição (osciloscópio) | P |
| RF08 | No modo de reconfiguração, o PC envia uma trama tipo MIDI por UART. | Trama recebida completa em 100 % de 10 envios | Ensaio | C |
| RF09 | O sistema valida a trama (cabeçalho, comprimento e checksum) antes de gravar. | 10 de 10 tramas inválidas rejeitadas, EEPROM inalterada | Ensaio | C |
| RF10 | Uma trama válida é gravada numa das 2 posições de preset da EEPROM e fica disponível como novo preset. | Preset novo tocável após gravar e após reiniciar | Ensaio | C |
| RF11 | O sistema responde ao PC com confirmação ("OK") ou erro ("ERRO") após cada trama. | 100 % das tramas com resposta em ≤ 200 ms | Ensaio + medição | P |
| RF12 | O sistema indica o modo ativo com LEDs (LED1 = modo 1, LED2 = modo 2, LED3 = modo 3). | 1 LED por modo, aceso só no modo ativo | Inspeção + ensaio | P |
| RF13 | Ao mudar de modo, o som atual é interrompido. | Som parado ≤ 100 ms após carregar em B3 | Medição (osciloscópio) | P |
| RF14 | O botão B3 muda de modo (1 preset, 2 live, 3 reconfiguração). | Cada pressão de B3 muda o modo e o LED correspondente acende | Ensaio | P |

## 2. Requisitos não funcionais (RNF)

| ID | Requisito | Critério | Verificação | Origem |
|----|-----------|----------|-------------|--------|
| RNF01 | Consumo de corrente. | ≤ 150 mA a 5 V com os 3 buzzers a tocar | Medição (multímetro em série) | P |
| RNF02 | Tolerância da alimentação. | Funciona com 5 V ± 5 % (4,75 V a 5,25 V) | Medição + ensaio | P |
| RNF03 | Tempo de arranque. | Pronto a receber um comando ≤ 1 s após ligar | Medição (cronómetro) | P |
| RNF04 | Fiabilidade das músicas guardadas. | 50 ciclos de ligar/desligar sem perda ou corrupção de presets | Ensaio | P |
| RNF05 | Qualidade da comunicação. | 0 erros em 100 mensagens enviadas a 9600 baud | Ensaio | P |
| RNF06 | Dimensões da PCB (2 camadas). | ≤ 80 mm × 60 mm (a confirmar com o cliente) | Medição | P |
| RNF07 | Custo dos componentes (BOM). | ≤ 20 € (a confirmar com o cliente) | Inspeção (BOM) | P |
| RNF08 | Documentação completa no repositório Git. | Esquema, BOM, código comentado e README presentes | Inspeção | P |
| RNF09 | Projeto do circuito em Altium sem erros de regras. | DRC com 0 erros | DRC | P |

## 3. Restrições (RE)

| ID | Restrição | Critério | Verificação | Origem |
|----|-----------|----------|-------------|--------|
| RE01 | Microcontrolador. | ATmega328P | Inspeção | C |
| RE02 | Número de buzzers. | 3 | Inspeção | C |
| RE03 | Alimentação por USB. | 5 V via USB, sem fonte externa | Inspeção + ensaio | C |
| RE04 | Comunicação série. | UART a 9600 baud | Ensaio | C |
| RE05 | Armazenamento das músicas. | EEPROM interna do ATmega328P (1024 bytes) | Inspeção | C |
| RE06 | Ferramentas de projeto. | Simulação em Tinkercad e esquema/PCB em Altium 365 | Inspeção | P |
| RE07 | Prazos de entrega. | E1: 26/10/2026; E2: 05/11/2026; E3: 14/12/2026; PF: 28/01/2027 | Inspeção | C |

## 4. Notas para validar com o cliente

- Os valores marcados com `P` (consumo, gama de notas, tempos, dimensões, custo) são propostas e devem ser confirmados ou ajustados.
- Capacidade da EEPROM: 2 presets de 64 eventos com 3 bytes por evento ocupam cerca de 384 bytes dos 1024 disponíveis. Ajustar se o formato da trama mudar.
- Falta fechar: orçamento e tamanho.
