#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
midi_para_trama.py - Grupo 8, LDH 2026/2027

Converte um ficheiro MIDI (.mid) numa trama para o modo 3 (reconfiguração) do
sistema musical com 3 buzzers e envia-a pela porta série.

Formato da trama (todos os valores em bytes):
    0xAA | SLOT | LEN | { CANAL NOTA DURACAO } x N | CHK
    SLOT     1..2     música (posição da EEPROM) onde o Arduino a guarda
    LEN      3 x N    (máximo 240)
    CANAL    1..3     buzzer que toca o evento
    NOTA     60..95   número de nota MIDI (0 = pausa)
    DURACAO  5..200   em unidades de 10 ms
    CHK      XOR de todos os bytes anteriores

Exemplos:
    python midi_para_trama.py musica.mid --slot 1 --hex          # mostra a trama em hexadecimal (Tinkercad)
    python midi_para_trama.py musica.mid --slot 2 --porta COM3   # envia os bytes ao Arduino real
    python midi_para_trama.py musica.mid --listar                # mostra os canais do ficheiro
    python midi_para_trama.py musica.mid --canais 1,2 --hex      # usa só os canais 1 e 2
    python midi_para_trama.py --exemplo --hex                    # trama de teste sem ficheiro MIDI

Nota: ao abrir a porta série o Arduino Uno reinicia e volta ao estado inicial, por isso o script
pede-te para carregares em B3 (modo 3) DEPOIS de abrir a porta e antes de enviar. Fecha o
Monitor Série do Arduino IDE antes de usar --porta (a porta só pode estar aberta num programa).

Instalação:  pip install mido pyserial
"""
import argparse
import sys

MAX_EV = 80          # eventos por preset (igual ao firmware)
NOTA_MIN, NOTA_MAX = 60, 95
DUR_MIN, DUR_MAX = 5, 200      # unidades de 10 ms
HDR = 0xAA


# ----------------------------------------------------------------------------
# Construção da trama
# ----------------------------------------------------------------------------
def montar_trama(slot, eventos):
    """eventos: lista de (canal 1..3, nota 0 ou 60..95, duracao 5..200)."""
    if not 1 <= slot <= 2:
        raise ValueError("o slot tem de ser 1 ou 2")
    if not 1 <= len(eventos) <= MAX_EV:
        raise ValueError(f"a música tem de ter entre 1 e {MAX_EV} eventos")
    corpo = [HDR, slot, 3 * len(eventos)]
    for canal, nota, dur in eventos:
        assert 1 <= canal <= 3 and (nota == 0 or NOTA_MIN <= nota <= NOTA_MAX) and DUR_MIN <= dur <= DUR_MAX
        corpo += [canal, nota, dur]
    chk = 0
    for b in corpo:
        chk ^= b
    return bytes(corpo + [chk])


def para_hex(trama):
    return " ".join(f"{b:02X}" for b in trama)


# ----------------------------------------------------------------------------
# Leitura do MIDI
# ----------------------------------------------------------------------------
def ler_notas(caminho, canais=None):
    """Devolve a lista de notas (inicio_s, fim_s, nota), ordenada por início.
    canais: canais MIDI a usar (1 a 16). Se for None usa todos menos o canal 10 (percussão)."""
    import mido
    usar = (lambda c: (c + 1) in canais) if canais else (lambda c: c != 9)
    mid = mido.MidiFile(caminho)
    t = 0.0
    abertas = {}                         # (canal, nota) -> lista de inícios
    notas = []
    for msg in mid:                      # mido já aplica as mudanças de tempo (tempo map)
        t += msg.time
        if msg.type == "note_on" and msg.velocity > 0:
            if usar(msg.channel):
                abertas.setdefault((msg.channel, msg.note), []).append(t)
        elif msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0):
            lst = abertas.get((msg.channel, msg.note))
            if lst:
                notas.append((lst.pop(0), t, msg.note))
    for (canal, nota), lst in abertas.items():      # notas sem note_off: duram até ao fim
        for ini in lst:
            notas.append((ini, t, nota))
    notas.sort(key=lambda n: (n[0], -n[2]))      # ao mesmo tempo: as mais agudas primeiro (a melodia fica)
    return notas


NOMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def nome_nota(n):
    return f"{NOMES[n % 12]}{n // 12 - 1}"


def listar(caminho):
    """Mostra os canais do ficheiro MIDI para escolheres quais usar com --canais."""
    import mido
    t = 0.0
    info = {}
    for msg in mido.MidiFile(caminho):
        t += msg.time
        if msg.type == "note_on" and msg.velocity > 0:
            d = info.setdefault(msg.channel, {"n": 0, "min": 127, "max": 0, "ini": t, "fim": t})
            d["n"] += 1; d["min"] = min(d["min"], msg.note); d["max"] = max(d["max"], msg.note); d["fim"] = t
    if not info:
        print("O ficheiro não tem notas.")
        return
    print("Canal  Notas  Gama           Início-fim (s)")
    for c in sorted(info):
        d = info[c]
        obs = "  (percussão: ignorado por omissão)" if c == 9 else ""
        print(f"{c + 1:>5}  {d['n']:>5}  {nome_nota(d['min']):>3} a {nome_nota(d['max']):<4}     {d['ini']:5.1f}-{d['fim']:.1f}{obs}")
    print("Usa --canais para escolher, por exemplo: --canais 1,2,3")


def encaixar_na_gama(nota):
    while nota < NOTA_MIN:
        nota += 12
    while nota > NOTA_MAX:
        nota -= 12
    return nota


def repartir_em_3_vozes(notas):
    """Cada buzzer só toca uma nota de cada vez: atribui cada nota à primeira voz livre.
    Se as 3 vozes estiverem ocupadas, a nota é descartada."""
    vozes = [[], [], []]
    livre_em = [0.0, 0.0, 0.0]
    descartadas = 0
    for ini, fim, nota in notas:
        for v in range(3):
            if livre_em[v] <= ini + 1e-6:
                vozes[v].append((ini, fim, nota))
                livre_em[v] = fim
                break
        else:
            descartadas += 1
    return vozes, descartadas


def voz_para_eventos(canal, voz, limite_s=None):
    """Converte as notas de uma voz em eventos (canal, nota, duracao) com pausas."""
    eventos = []
    cursor = 0.0
    for ini, fim, nota in voz:
        if limite_s is not None and ini >= limite_s:
            break
        fim = min(fim, limite_s) if limite_s is not None else fim
        pausa = ini - cursor
        if pausa >= DUR_MIN * 0.01:                       # pausas < 50 ms ignoram-se
            eventos += _partir(canal, 0, pausa)
            cursor += pausa
        dur = max(fim - ini, DUR_MIN * 0.01)
        eventos += _partir(canal, encaixar_na_gama(nota), dur)
        cursor += dur
    return eventos


def _partir(canal, nota, segundos):
    """Divide em vários eventos se passar de 2000 ms."""
    unidades = int(round(segundos * 100))
    out = []
    while unidades > 0:
        u = min(unidades, DUR_MAX)
        if u < DUR_MIN:
            break
        out.append((canal, nota, u))
        unidades -= u
    return out


def midi_para_eventos(caminho, canais=None):
    notas = ler_notas(caminho, canais)
    if not notas:
        raise ValueError("o ficheiro MIDI não tem notas")
    vozes, descartadas = repartir_em_3_vozes(notas)
    if descartadas:
        print(f"Aviso: {descartadas} notas descartadas (mais de 3 ao mesmo tempo).", file=sys.stderr)

    # Se houver eventos a mais, corta a música no mesmo instante em todas as vozes
    limite = None
    while True:
        eventos = []
        for v in range(3):
            eventos += voz_para_eventos(v + 1, vozes[v], limite)
        if len(eventos) <= MAX_EV:
            break
        fim_total = max(f for voz in vozes for _, f, _ in voz) if limite is None else limite
        limite = fim_total - 0.5
        if limite <= 0:
            raise ValueError("não foi possível reduzir a música para caber na EEPROM")
    if limite is not None:
        print(f"Aviso: música cortada aos {limite:.1f} s para caber em {MAX_EV} eventos.", file=sys.stderr)
    if not eventos:
        raise ValueError("a música ficou sem eventos")
    return eventos


def exemplo():
    """Acorde C maior (C5 E5 G5) durante 500 ms, depois outro acorde (D5 F5 A5)."""
    ev = [(1, 72, 50), (2, 76, 50), (3, 79, 50), (1, 74, 50), (2, 77, 50), (3, 81, 50)]
    return ev


# ----------------------------------------------------------------------------
# Envio pela porta série
# ----------------------------------------------------------------------------
def ler_resposta(ser, tempo=4.0):
    """Lê linhas até aparecer uma que comece por OK ou ERRO (ignora as outras mensagens do sistema)."""
    import time
    fim = time.time() + tempo
    while time.time() < fim:
        linha = ser.readline().decode("utf-8", errors="replace").strip()
        if linha.startswith("OK") or linha.startswith("ERRO"):
            return linha
    return ""


def enviar(trama, porta, baud=9600, pausa=True):
    import time
    import serial          # pyserial
    with serial.serial_for_url(porta, baudrate=baud, timeout=0.5) as s:
        time.sleep(2)      # o Arduino reinicia quando a porta abre
        s.reset_input_buffer()
        if pausa:
            input("A placa reiniciou ao abrir a porta. Carrega em B3 (acende o LED 3) e prime Enter para enviar... ")
            s.reset_input_buffer()
        s.write(trama)
        s.flush()
        resposta = ler_resposta(s)
    print("Resposta do sistema:", resposta or "(sem resposta: a placa estava no modo 3?)")
    return resposta.startswith("OK")


def main():
    ap = argparse.ArgumentParser(description="Converte um ficheiro MIDI numa trama para o modo 3.")
    ap.add_argument("midi", nargs="?", help="ficheiro .mid")
    ap.add_argument("--slot", type=int, default=1, choices=[1, 2], help="música onde guardar (1 ou 2)")
    ap.add_argument("--hex", action="store_true", help="mostra a trama em hexadecimal (para colar no Tinkercad)")
    ap.add_argument("--porta", help="porta série (ex.: COM3 ou /dev/ttyUSB0) para enviar os bytes")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--sem-pausa", action="store_true", help="não pede para carregar em B3 (só se a placa não reiniciar ao abrir a porta)")
    ap.add_argument("--saida", help="guarda a trama binária num ficheiro")
    ap.add_argument("--listar", action="store_true", help="mostra os canais do ficheiro MIDI e sai")
    ap.add_argument("--canais", help="canais MIDI a usar (1 a 16), separados por vírgulas. Por omissão usa todos menos o 10 (percussão)")
    ap.add_argument("--exemplo", action="store_true", help="usa uma música de teste em vez de um ficheiro MIDI")
    a = ap.parse_args()

    canais = None
    if a.canais:
        try:
            canais = {int(x) for x in a.canais.split(",")}
            assert all(1 <= c <= 16 for c in canais)
        except Exception:
            ap.error("--canais deve ser uma lista de números de 1 a 16, por exemplo 1,2,3")
    if a.listar:
        if not a.midi:
            ap.error("indica o ficheiro .mid para listar")
        listar(a.midi)
        return
    if a.exemplo:
        eventos = exemplo()
    elif a.midi:
        eventos = midi_para_eventos(a.midi, canais)
    else:
        ap.error("indica um ficheiro .mid ou usa --exemplo")

    trama = montar_trama(a.slot, eventos)
    print(f"{len(eventos)} eventos, trama com {len(trama)} bytes, preset {a.slot}.", file=sys.stderr)
    if a.hex or not (a.porta or a.saida):
        print(para_hex(trama))
    if a.saida:
        open(a.saida, "wb").write(trama)
    if a.porta:
        sys.exit(0 if enviar(trama, a.porta, a.baud, not a.sem_pausa) else 1)


if __name__ == "__main__":
    main()
