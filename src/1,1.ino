// =======================================================
// 3 BOTOES, 3 LEDS, 3 BUZZERS
// MODO 2: enviar com B1_200_4 B3_300_2 (buzzer_freq_tempo)
// MODO 1: tocar simultaneamente todos os buzzers
// MODO 3: ler ficheiro MIDI e tocar sequencialmente
// =======================================================

#include <Arduino.h>
#include <MIDI.h>

// Definições dos pinos dos buzzers
#define BUZZER1_PIN 8
#define BUZZER2_PIN 9
#define BUZZER3_PIN 10

// Definições dos pinos dos botões
#define BUTTON1_PIN 2
#define BUTTON2_PIN 3
#define BUTTON3_PIN 4

// Definições dos pinos dos LEDs
#define LED1_PIN 5
#define LED2_PIN 6
#define LED3_PIN 7

// Estados dos buzzers
struct BuzzerState {
  int frequency;
  int duration;
  bool isPlaying;
};

BuzzerState buzzers[3] = {{0, 0, false}, {0, 0, false}, {0, 0, false}};

// Variáveis para controle do modo
int currentMode = 1; // 1: modo 1, 2: modo 2, 3: modo 3

// Timer para controle dos buzzers
unsigned long startTime[3] = {0, 0, 0};
unsigned long durations[3] = {0, 0, 0};

// Variáveis para MIDI
bool midiFileLoaded = false;
int currentNoteIndex = 0;

// Função para tocar um buzzer individualmente (para o modo 1)
void playSingleBuzzer(int buzzerIndex, int frequency, int duration) {
  if (buzzerIndex >= 0 && buzzerIndex < 3) {
    buzzers[buzzerIndex].frequency = frequency;
    buzzers[buzzerIndex].duration = duration;
    buzzers[buzzerIndex].isPlaying = true;
    
    // Configurar o timer
    startTime[buzzerIndex] = millis();
    durations[buzzerIndex] = duration * 1000; // converter segundos para milissegundos
    
    // Ativar o buzzer
    pinMode(BUZZER1_PIN + buzzerIndex, OUTPUT);
  }
}

// Função para tocar todos os buzzers simultaneamente (modo 1)
void playAllBuzzersSimultaneously() {
  // Para todos os buzzers
  for (int i = 0; i < 3; i++) {
    if (buzzers[i].isPlaying) {
      pinMode(BUZZER1_PIN + i, OUTPUT);
      tone(BUZZER1_PIN + i, buzzers[i].frequency);
    }
  }
}

// Função para parar todos os buzzers
void stopAllBuzzers() {
  for (int i = 0; i < 3; i++) {
    buzzers[i].isPlaying = false;
    noTone(BUZZER1_PIN + i);
    pinMode(BUZZER1_PIN + i, INPUT);
  }
}

// Função para processar comandos do modo 2
void processCommand(String command) {
  // Exemplo de comando: "B1_200_4 B3_300_2"
  
  int pos = 0;
  while (pos < command.length()) {
    // Encontrar o próximo comando
    int start = command.indexOf('B', pos);
    if (start == -1) break;
    
    int end = command.indexOf(' ', start);
    if (end == -1) end = command.length();
    
    String cmd = command.substring(start, end);
    
    // Processar o comando B1_200_4
    if (cmd.startsWith("B")) {
      int buzzerNum = cmd.charAt(1) - '0' - 1; // Convertendo para índice 0-2
      
      if (buzzerNum >= 0 && buzzerNum < 3) {
        int freqStart = cmd.indexOf('_', 2);
        int freqEnd = cmd.indexOf('_', freqStart + 1);
        
        if (freqStart != -1 && freqEnd != -1) {
          String frequencyStr = cmd.substring(freqStart + 1, freqEnd);
          String durationStr = cmd.substring(freqEnd + 1);
          
          int frequency = frequencyStr.toInt();
          int duration = durationStr.toInt();
          
          // Armazenar para uso posterior (modo 1)
          buzzers[buzzerNum].frequency = frequency;
          buzzers[buzzerNum].duration = duration;
          buzzers[buzzerNum].isPlaying = true;
        }
      }
    }
    
    pos = end + 1;
  }
}

// Função para verificar se todos os buzzers terminaram
bool allBuzzersFinished() {
  for (int i = 0; i < 3; i++) {
    if (buzzers[i].isPlaying) {
      return false;
    }
  }
  return true;
}

// Simulação de leitura de MIDI - substitua esta parte com a implementação real
void loadMIDIFile() {
  // Esta função simula o carregamento de um arquivo MIDI
  // Em uma implementação real, você precisaria usar uma biblioteca MIDI
  // e ter arquivos MIDI armazenados no storage do Arduino (SD card, etc.)
  
  Serial.println("Arquivo MIDI carregado com sucesso!");
  midiFileLoaded = true;
}

// Função para tocar notas do MIDI
void playMIDINote(int note, int duration) {
  // Mapear nota MIDI para frequência
  float frequency = 440.0 * pow(2, (note - 69) / 12.0);
  
  // Tocar a nota em todos os buzzers simultaneamente
  for (int i = 0; i < 3; i++) {
    if (buzzers[i].isPlaying) {
      tone(BUZZER1_PIN + i, frequency);
    }
  }
  
  delay(duration * 1000); // Duração em segundos
  
  // Parar todos os buzzers
  stopAllBuzzers();
}

void setup() {
  // Inicializar pinos
  pinMode(BUZZER1_PIN, OUTPUT);
  pinMode(BUZZER2_PIN, OUTPUT);
  pinMode(BUZZER3_PIN, OUTPUT);
  
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  
  // Inicializar serial
  Serial.begin(9600);
  Serial.println("Sistema pronto. Envie comandos no formato B1_200_4 B3_300_2");
  
  // Carregar arquivo MIDI (simulação)
  loadMIDIFile();
}

void loop() {
  // Verificar botões para mudar de modo
  if (digitalRead(BUTTON1_PIN) == LOW) {
    currentMode = 1;
    delay(200); // Debounce
  }
  
  if (digitalRead(BUTTON2_PIN) == LOW) {
    currentMode = 2;
    delay(200); // Debounce
  }
  
  if (digitalRead(BUTTON3_PIN) == LOW) {
    currentMode = 3;
    delay(200); // Debounce
  }

  // Processar modo 1: Tocar simultaneamente
  if (currentMode == 1) {
    playAllBuzzersSimultaneously();
  }
  
  // Processar modo 2: Entrada de comandos
  else if (currentMode == 2) {
    if (Serial.available() > 0) {
      String input = Serial.readString();
      processCommand(input);
    }
  }
  
  // Processar modo 3: Tocar MIDI
  else if (currentMode == 3) {
    if (midiFileLoaded) {
      // Simulação de reprodução de notas MIDI
      // Em uma implementação real, você faria:
      /*
      for (int i = 0; i < midiNoteCount; i++) {
        int note = midiNotes[i];
        int duration = midiDurations[i];
        playMIDINote(note, duration);
        delay(100); // Intervalo entre notas
      }
      */
      
      // Simulação de reprodução
      Serial.println("Reproduzindo MIDI...");
      for (int i = 0; i < 5; i++) {
        int note = 60 + i * 2; // Notas progressivas
        int duration = 1;
        playMIDINote(note, duration);
        delay(500);
      }
      midiFileLoaded = false; // Reiniciar para próxima reprodução
    }
  }

  // Controle de LEDs
  if (currentMode == 1) {
    digitalWrite(LED1_PIN, HIGH);
    digitalWrite(LED2_PIN, LOW);
    digitalWrite(LED3_PIN, LOW);
  }
  else if (currentMode == 2) {
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, HIGH);
    digitalWrite(LED3_PIN, LOW);
  }
  else if (currentMode == 3) {
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, LOW);
    digitalWrite(LED3_PIN, HIGH);
  }

  delay(100);
}