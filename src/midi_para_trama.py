#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
midi_para_trama.py - Grupo 8, LDH 2026/2027

Converte um ficheiro MIDI (.mid) numa trama para o modo 3 (reconfiguração) do
sistema musical com 3 buzzers e envia-a pela porta série.

Formato da trama (todos os valores em bytes):
    0xAA | SLOT | LEN | { CANAL NOTA DURACAO } x N | CHK
    SLOT     1..3     preset onde o Arduino guarda a música
    LEN      3 x N    (máximo 240)
    CANAL    1..3     buzzer que toca o evento
    NOTA     60..95   número de nota MIDI (0 = pausa)
    DURACAO  5..200   em unidades de 10 ms
    CHK      XOR de todos os bytes anteriores

Exemplos:
    python midi_para_trama.py musica.mid --slot 1 --hex          # mostra a trama em hexadecimal (Tinkercad)
    python midi_para_trama.py musica.mid --slot 2 --porta COM3   # envia os bytes ao Arduino real
    python midi_para_trama.py --exemplo --hex                    # trama de teste sem ficheiro MIDI

Nota: ao abrir a porta série o Arduino Uno reinicia e volta ao estado inicial, por isso o script
pede-te para carregares em B3 (modo 3) DEPOIS de abrir a porta e antes de enviar. Fecha o
Monitor Série do Arduino IDE antes de usar --porta (a porta só pode estar aberta num programa).

Instalação:  pip install mido pyserial
Áudio→MIDI:  pip install librosa soundfile numpy
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
    if not 1 <= slot <= 3:
        raise ValueError("o slot tem de ser 1, 2 ou 3")
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
def ler_notas(caminho):
    """Devolve a lista de notas (inicio_s, fim_s, nota) de todo o ficheiro, ordenada por início."""
    import mido
    mid = mido.MidiFile(caminho)
    t = 0.0
    abertas = {}                         # (canal, nota) -> lista de inícios
    notas = []
    for msg in mid:                      # mido já aplica as mudanças de tempo (tempo map)
        t += msg.time
        if msg.type == "note_on" and msg.velocity > 0:
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


def midi_para_eventos(caminho):
    notas = ler_notas(caminho)
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
# Conversão de áudio (MP3/WAV/OGG) para MIDI
# ----------------------------------------------------------------------------
def mp3_para_midi(entrada, saida, progresso=None):
    """Transcreve um ficheiro de áudio para MIDI usando deteção de pitch.
    Requer: pip install librosa soundfile mido numpy
    Devolve o número de notas escritas."""
    import numpy as np
    import librosa
    import mido

    if progresso:
        progresso("A carregar áudio…")
    y, sr = librosa.load(entrada, sr=22050, mono=True)

    if progresso:
        progresso("A detetar pitch (pYIN)…")
    f0, voiced_flag, _ = librosa.pyin(
        y, fmin=librosa.note_to_hz("C3"), fmax=librosa.note_to_hz("B6"),
        sr=sr, frame_length=2048
    )
    tempos = librosa.times_like(f0, sr=sr)

    if progresso:
        progresso("A agrupar notas…")
    # Agrupa frames consecutivos com a mesma nota (arredondada a semitom MIDI)
    notas = []              # (inicio_s, fim_s, midi_note)
    atual_nota = None
    inicio = None
    for t, f, v in zip(tempos, f0, voiced_flag):
        if v and not np.isnan(f):
            midi_n = int(round(librosa.hz_to_midi(f)))
            if atual_nota is None:
                atual_nota, inicio = midi_n, t
            elif midi_n != atual_nota:
                notas.append((inicio, t, atual_nota))
                atual_nota, inicio = midi_n, t
        else:
            if atual_nota is not None:
                notas.append((inicio, t, atual_nota))
                atual_nota, inicio = None, None
    if atual_nota is not None:
        notas.append((inicio, tempos[-1] if len(tempos) else 0.0, atual_nota))

    # Filtra notas demasiado curtas (< 80 ms) — ruído
    notas = [n for n in notas if n[1] - n[0] >= 0.08]
    if not notas:
        raise ValueError("Não foram detetadas notas no áudio.")

    if progresso:
        progresso(f"A escrever {len(notas)} notas no MIDI…")
    mid = mido.MidiFile()
    track = mido.MidiTrack()
    mid.tracks.append(track)
    track.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(120)))

    # Converte segundos para ticks (480 ticks por batida, 120 bpm)
    ticks_por_seg = mid.ticks_per_beat * 2   # 120 bpm => 2 batidas/seg
    eventos = []
    for ini, fim, nota in notas:
        eventos.append((int(ini * ticks_por_seg), "on", nota))
        eventos.append((int(fim * ticks_por_seg), "off", nota))
    eventos.sort(key=lambda e: (e[0], e[1] == "on"))  # off antes de on no mesmo instante

    ultimo = 0
    for tick, tipo, nota in eventos:
        delta = tick - ultimo
        ultimo = tick
        if tipo == "on":
            track.append(mido.Message("note_on", note=nota, velocity=100, time=delta))
        else:
            track.append(mido.Message("note_off", note=nota, velocity=0, time=delta))

    mid.save(saida)
    return len(notas)


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


def enviar(trama, porta, baud=9600, pausa=True, antes_de_enviar=None):
    import time
    import serial          # pyserial
    with serial.serial_for_url(porta, baudrate=baud, timeout=0.5) as s:
        time.sleep(2)      # o Arduino reinicia quando a porta abre
        s.reset_input_buffer()
        if pausa:
            if antes_de_enviar is not None:
                antes_de_enviar()
            else:
                input("A placa reiniciou ao abrir a porta. Carrega em B3 (acende o LED 3) e prime Enter para enviar... ")
            s.reset_input_buffer()
        s.write(trama)
        s.flush()
        resposta = ler_resposta(s)
    print("Resposta do sistema:", resposta or "(sem resposta: a placa estava no modo 3?)")
    return resposta.startswith("OK")



# ----------------------------------------------------------------------------
# Interface gráfica
# ----------------------------------------------------------------------------
def main():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    from pathlib import Path

    class Aplicacao(tk.Tk):
        def __init__(self):
            super().__init__()
            self.title("MIDI para Trama — Grupo 8")
            self.geometry("760x650")
            self.minsize(680, 570)
            self.configure(bg="#f2f4f7")
            self.midi_path = tk.StringVar()
            self.slot = tk.IntVar(value=1)
            self.porta = tk.StringVar()
            self.baud = tk.StringVar(value="9600")
            self.sem_pausa = tk.BooleanVar(value=False)
            self.trama = None
            self.eventos = None
            self._estilo()
            self._interface()
            self._listar_portas()

        def _estilo(self):
            style = ttk.Style(self)
            try:
                style.theme_use("clam")
            except tk.TclError:
                pass
            style.configure("TFrame", background="#f2f4f7")
            style.configure("Card.TFrame", background="white")
            style.configure("TLabel", background="#f2f4f7", foreground="#202938", font=("Segoe UI", 10))
            style.configure("Title.TLabel", font=("Segoe UI", 19, "bold"), foreground="#152238")
            style.configure("Sub.TLabel", font=("Segoe UI", 9), foreground="#637083")
            style.configure("TButton", font=("Segoe UI", 10), padding=(10, 7))
            style.configure("Accent.TButton", font=("Segoe UI", 10, "bold"), padding=(12, 8))
            style.configure("TLabelframe", background="white")
            style.configure("TLabelframe.Label", background="white", foreground="#26364d", font=("Segoe UI", 10, "bold"))
            style.configure("TEntry", padding=6)
            style.configure("TCombobox", padding=5)

        def _interface(self):
            root = ttk.Frame(self, padding=18)
            root.pack(fill="both", expand=True)
            ttk.Label(root, text="MIDI para Trama", style="Title.TLabel").pack(anchor="w")
            ttk.Label(root, text="Converte músicas MIDI e envia os dados para o Arduino.", style="Sub.TLabel").pack(anchor="w", pady=(2, 14))

            ficheiro = ttk.LabelFrame(root, text="1. Música de entrada", padding=12)
            ficheiro.pack(fill="x", pady=(0, 10))
            linha = ttk.Frame(ficheiro)
            linha.pack(fill="x")
            ttk.Entry(linha, textvariable=self.midi_path).pack(side="left", fill="x", expand=True)
            ttk.Button(linha, text="Procurar MIDI…", command=self._escolher_midi).pack(side="left", padx=(8, 0))
            ttk.Button(linha, text="Usar música de teste", command=self._usar_exemplo).pack(side="left", padx=(8, 0))
            ttk.Button(linha, text="Converter MP3 → MIDI…", command=self._converter_mp3).pack(side="left", padx=(8, 0))

            config = ttk.LabelFrame(root, text="2. Configuração", padding=12)
            config.pack(fill="x", pady=(0, 10))
            linha = ttk.Frame(config)
            linha.pack(fill="x")
            ttk.Label(linha, text="Preset / Slot:").pack(side="left")
            for n in (1, 2, 3):
                ttk.Radiobutton(linha, text=str(n), variable=self.slot, value=n).pack(side="left", padx=5)
            ttk.Button(linha, text="Converter música", style="Accent.TButton", command=self._converter).pack(side="right")

            envio = ttk.LabelFrame(root, text="3. Envio para Arduino (opcional)", padding=12)
            envio.pack(fill="x", pady=(0, 10))
            linha = ttk.Frame(envio)
            linha.pack(fill="x")
            ttk.Label(linha, text="Porta série:").pack(side="left")
            self.combo_portas = ttk.Combobox(linha, textvariable=self.porta, width=18)
            self.combo_portas.pack(side="left", padx=(7, 12))
            ttk.Button(linha, text="Atualizar portas", command=self._listar_portas).pack(side="left")
            ttk.Label(linha, text="Baud:").pack(side="left", padx=(18, 5))
            ttk.Entry(linha, textvariable=self.baud, width=10).pack(side="left")
            ttk.Checkbutton(envio, text="Não esperar pelo botão B3 (usar apenas se a placa não reiniciar)", variable=self.sem_pausa).pack(anchor="w", pady=(8, 0))
            ttk.Button(envio, text="Enviar trama para Arduino", style="Accent.TButton", command=self._enviar).pack(anchor="e", pady=(8, 0))

            resultado = ttk.LabelFrame(root, text="4. Resultado da conversão", padding=10)
            resultado.pack(fill="both", expand=True, pady=(0, 10))
            self.info = ttk.Label(resultado, text="Ainda não foi gerada nenhuma trama.", style="Sub.TLabel")
            self.info.pack(anchor="w", pady=(0, 7))
            self.texto = tk.Text(resultado, height=8, wrap="word", font=("Consolas", 10), bg="#111827", fg="#d7e3f4", insertbackground="white", relief="flat", padx=10, pady=10)
            self.texto.pack(fill="both", expand=True)
            self.texto.insert("1.0", "A trama hexadecimal aparecerá aqui…")
            self.texto.configure(state="disabled")
            botoes = ttk.Frame(resultado)
            botoes.pack(fill="x", pady=(8, 0))
            ttk.Button(botoes, text="Copiar hexadecimal", command=self._copiar).pack(side="left")
            ttk.Button(botoes, text="Guardar trama .bin…", command=self._guardar).pack(side="left", padx=8)

            self.estado = tk.StringVar(value="Pronto.")
            ttk.Label(root, textvariable=self.estado, style="Sub.TLabel").pack(anchor="w")

        def _escolher_midi(self):
            caminho = filedialog.askopenfilename(
                title="Escolher ficheiro MIDI",
                filetypes=[("Ficheiros MIDI", "*.mid *.midi"), ("Todos os ficheiros", "*.*")]
            )
            if caminho:
                self.midi_path.set(caminho)
                self.eventos = None
                self.trama = None
                self.estado.set("Ficheiro selecionado. Carrega em «Converter música».")

        def _usar_exemplo(self):
            self.midi_path.set("")
            self.eventos = exemplo()
            self._criar_trama()
            self.estado.set("Música de teste carregada e convertida.")

        def _converter_mp3(self):
            """Pede ficheiro de áudio e destino MIDI, e converte numa thread."""
            entrada = filedialog.askopenfilename(
                title="Escolher ficheiro de áudio para converter",
                filetypes=[("Áudio", "*.mp3 *.wav *.ogg *.flac *.m4a"), ("Todos os ficheiros", "*.*")]
            )
            if not entrada:
                return
            saida = filedialog.asksaveasfilename(
                title="Guardar MIDI resultante",
                defaultextension=".mid",
                filetypes=[("Ficheiros MIDI", "*.mid"), ("Todos os ficheiros", "*.*")],
                initialfile=Path(entrada).stem + ".mid"
            )
            if not saida:
                return

            self.estado.set("A converter áudio para MIDI… (pode demorar alguns minutos)")
            self.config(cursor="watch")
            self.update_idletasks()

            def tarefa():
                try:
                    n = mp3_para_midi(entrada, saida, progresso=lambda m: self.estado.set(m))
                    self.after(0, lambda: self._fim_mp3(saida, n))
                except Exception as exc:
                    self.after(0, lambda: self._erro_mp3(exc))

            import threading
            threading.Thread(target=tarefa, daemon=True).start()

        def _fim_mp3(self, caminho_midi, n_notas):
            self.config(cursor="")
            self.midi_path.set(caminho_midi)
            self.eventos = None
            self.trama = None
            self.estado.set(f"Áudio convertido: {n_notas} notas. Carrega em «Converter música».")
            messagebox.showinfo(
                "Conversão MP3 → MIDI concluída",
                f"Foram escritas {n_notas} notas em:\n{caminho_midi}\n\n"
                "Agora carrega em «Converter música» para gerar a trama."
            )

        def _erro_mp3(self, exc):
            self.config(cursor="")
            self.estado.set("A conversão MP3 → MIDI falhou.")
            messagebox.showerror(
                "Erro na conversão de áudio",
                f"{exc}\n\nGarante que tens instalado:\n    pip install librosa soundfile mido numpy\n"
                "e que o ficheiro de áudio não está corrompido."
            )

        def _converter(self):
            try:
                caminho = self.midi_path.get().strip()
                if not caminho:
                    raise ValueError("Escolhe um ficheiro MIDI ou usa a música de teste.")
                if not Path(caminho).is_file():
                    raise ValueError("O ficheiro selecionado já não existe.")
                self.eventos = midi_para_eventos(caminho)
                self._criar_trama()
                self.estado.set("Conversão concluída com sucesso.")
            except Exception as exc:
                messagebox.showerror("Erro na conversão", str(exc))
                self.estado.set("A conversão não foi concluída.")

        def _criar_trama(self):
            self.trama = montar_trama(self.slot.get(), self.eventos)
            hexa = para_hex(self.trama)
            self.info.configure(text=f"{len(self.eventos)} eventos  |  {len(self.trama)} bytes  |  Preset {self.slot.get()}")
            self.texto.configure(state="normal")
            self.texto.delete("1.0", "end")
            self.texto.insert("1.0", hexa)
            self.texto.configure(state="disabled")

        def _garantir_trama(self):
            if self.trama is None:
                messagebox.showwarning("Sem trama", "Primeiro converte um MIDI ou carrega a música de teste.")
                return False
            # O slot pode ter sido alterado depois da conversão.
            try:
                self._criar_trama()
            except Exception as exc:
                messagebox.showerror("Erro", str(exc))
                return False
            return True

        def _copiar(self):
            if not self._garantir_trama():
                return
            self.clipboard_clear()
            self.clipboard_append(para_hex(self.trama))
            self.estado.set("Hexadecimal copiado para a área de transferência.")

        def _guardar(self):
            if not self._garantir_trama():
                return
            caminho = filedialog.asksaveasfilename(
                title="Guardar trama binária",
                defaultextension=".bin",
                filetypes=[("Ficheiro binário", "*.bin"), ("Todos os ficheiros", "*.*")]
            )
            if caminho:
                try:
                    Path(caminho).write_bytes(self.trama)
                    self.estado.set(f"Trama guardada: {caminho}")
                    messagebox.showinfo("Guardado", "A trama binária foi guardada com sucesso.")
                except Exception as exc:
                    messagebox.showerror("Erro ao guardar", str(exc))

        def _listar_portas(self):
            try:
                from serial.tools import list_ports
                portas = [p.device for p in list_ports.comports()]
                self.combo_portas["values"] = portas
                if portas and not self.porta.get():
                    self.porta.set(portas[0])
                self.estado.set(f"{len(portas)} porta(s) série encontrada(s)." if portas else "Nenhuma porta série encontrada.")
            except ImportError:
                self.combo_portas["values"] = []
                self.estado.set("Instala pyserial para detetar portas série.")

        def _enviar(self):
            if not self._garantir_trama():
                return
            porta = self.porta.get().strip()
            if not porta:
                messagebox.showwarning("Porta em falta", "Seleciona a porta série do Arduino.")
                return
            try:
                baud = int(self.baud.get())
                if baud <= 0:
                    raise ValueError("A velocidade baud tem de ser positiva.")
            except ValueError:
                messagebox.showerror("Baud inválido", "Introduz uma velocidade baud válida, por exemplo 9600.")
                return

            if not messagebox.askyesno(
                "Confirmar envio",
                f"Vou abrir {porta} a {baud} baud.\n\nO Arduino vai reiniciar ao abrir a porta. "
                "Fecha o Monitor Série do Arduino IDE.\n\nContinuar?"
            ):
                return
            try:
                ok = enviar(
                    self.trama, porta, baud, not self.sem_pausa.get(),
                    antes_de_enviar=lambda: messagebox.showinfo(
                        "Preparar Arduino",
                        "A placa reiniciou ao abrir a porta.\n\nCarrega em B3 (deve acender o LED 3) e prime OK para enviar a trama."
                    )
                )
                if ok:
                    messagebox.showinfo("Envio concluído", "O Arduino confirmou a receção da trama.")
                    self.estado.set("Envio confirmado pelo Arduino.")
                else:
                    messagebox.showwarning("Sem confirmação", "A trama foi enviada, mas o Arduino não respondeu com OK. Confirma que está no modo 3.")
                    self.estado.set("Envio sem confirmação do Arduino.")
            except Exception as exc:
                messagebox.showerror("Erro no envio", str(exc))
                self.estado.set("O envio falhou.")

    app = Aplicacao()
    app.mainloop()


if __name__ == "__main__":
    main()