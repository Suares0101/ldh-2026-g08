# Problem Statement

**Projeto:** Sistema musical com 3 buzzers (ATmega328P)
**Grupo:** [nomes e números]
**Data:** [data]

## 1. Contexto

- Os buzzers são usados em muitos sistemas embebidos, mas quase sempre para um único som ou aviso.
- Os exemplos existentes de música com vários buzzers (ver `estado-da-arte.md`) tocam uma música fixa no código, sem escolha pelo utilizador, sem comunicação com o PC e sem guardar músicas novas.
- O cliente pretende um sistema simples, de baixo custo, alimentado por USB, que toque música em 3 buzzers e permita mudar as músicas sem reprogramar o microcontrolador.

## 2. Problema

- Falta um sistema compacto que toque música em 3 vozes e seja flexível: escolher músicas guardadas, tocar notas em tempo real e atualizar as músicas guardadas.
- Hoje, mudar uma música obriga a alterar o código e a reprogramar o ATmega328P.

## 3. Utilizadores

- **Utilizador final:** quem usa o sistema no dia a dia, escolhe e ouve as músicas através dos botões.
- **Utilizador técnico:** quem liga o sistema ao PC por USB para tocar notas pelo terminal ou para enviar músicas novas.

## 4. Objetivo

- Desenvolver um sistema com ATmega328P que toca música em 3 buzzers e funciona em 3 modos:
  - **Preset:** um botão seleciona e toca uma música guardada na EEPROM.
  - **Live:** o PC envia por UART a nota e a duração, que tocam em tempo real.
  - **Reconfiguração:** o PC envia uma trama tipo MIDI que o sistema valida e grava na EEPROM, ficando disponível como novo preset.

## 5. Âmbito

**Inclui:**
- Hardware com ATmega328P, 3 buzzers e 3 botões (um por modo), alimentado por USB.
- Comunicação UART com o PC a 9600 baud.
- Armazenamento de músicas na EEPROM e validação das tramas recebidas.
- Simulação do circuito (Tinkercad) e montagem física em laboratório.
- Documentação e repositório Git.

**Exclui:**
- Comunicação sem fios (Bluetooth, Wi-Fi).
- Controlo de volume e qualidade de som de alta fidelidade.
- Aplicação gráfica no PC (usa-se um terminal série).
- Caixa final e produção em série.
- [outros pontos a confirmar com o cliente: orçamento, tamanho, prazo]

## 6. Sucesso

- Os 3 buzzers tocam em simultâneo, cada um com a sua voz.
- No modo preset, o botão seleciona e toca a música guardada, e esta persiste depois de desligar.
- No modo live, as notas enviadas por UART tocam em tempo real com a duração pedida.
- No modo de reconfiguração, uma trama válida é gravada na EEPROM e uma inválida é rejeitada sem corromper as músicas já guardadas.
- O sistema funciona só com alimentação USB.
- [critério de custo/prazo, a confirmar com o cliente]
