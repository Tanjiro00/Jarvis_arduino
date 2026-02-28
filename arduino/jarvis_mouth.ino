#include "LedControl.h"

// === ПИНЫ ===
// MAX7219: DIN=12, CLK=11, CS=10
LedControl lc = LedControl(12, 11, 10, 1);

// HC-SR04
const int TRIG_PIN = 7;
const int ECHO_PIN = 6;

// === НАСТРОЙКИ ===
const int WAKE_DISTANCE_CM = 100;    // порог активации (см)
const unsigned long SLEEP_DELAY = 10000; // задержка перед сном (мс)
const unsigned long MEASURE_INTERVAL = 200; // интервал замера расстояния (мс)

// === БИТМАПЫ РТА 8x8 ===
// Каждый массив — 8 строк (байт), бит = светодиод

// M0: рот закрыт — горизонтальная линия
const byte MOUTH_CLOSED[8] = {
  B00000000,
  B00000000,
  B00000000,
  B00111100,
  B00000000,
  B00000000,
  B00000000,
  B00000000
};

// M1: слегка приоткрыт
const byte MOUTH_SMALL[8] = {
  B00000000,
  B00000000,
  B00111100,
  B01000010,
  B00111100,
  B00000000,
  B00000000,
  B00000000
};

// M2: средне открыт
const byte MOUTH_MEDIUM[8] = {
  B00000000,
  B00111100,
  B01000010,
  B01000010,
  B01000010,
  B00111100,
  B00000000,
  B00000000
};

// M3: широко открыт
const byte MOUTH_WIDE[8] = {
  B00000000,
  B00111100,
  B01000010,
  B10000001,
  B10000001,
  B01000010,
  B00111100,
  B00000000
};

// M4: очень широко
const byte MOUTH_FULL[8] = {
  B00111100,
  B01000010,
  B10000001,
  B10000001,
  B10000001,
  B10000001,
  B01000010,
  B00111100
};

// Слушаю — кадр 1 (маленькая точка)
const byte LISTEN_1[8] = {
  B00000000,
  B00000000,
  B00000000,
  B00011000,
  B00011000,
  B00000000,
  B00000000,
  B00000000
};

// Слушаю — кадр 2 (средний круг)
const byte LISTEN_2[8] = {
  B00000000,
  B00000000,
  B00111100,
  B00100100,
  B00100100,
  B00111100,
  B00000000,
  B00000000
};

// Слушаю — кадр 3 (большой круг)
const byte LISTEN_3[8] = {
  B00000000,
  B00111100,
  B01000010,
  B01000010,
  B01000010,
  B01000010,
  B00111100,
  B00000000
};

// Сон — zzZ
const byte SLEEP_FACE[8] = {
  B00000000,
  B00000000,
  B00011110,
  B00000100,
  B00001000,
  B00011110,
  B00000000,
  B00000000
};

// === МАССИВ КАДРОВ ДЛЯ РАЗГОВОРА ===
const byte* TALK_FRAMES[] = {MOUTH_CLOSED, MOUTH_SMALL, MOUTH_MEDIUM, MOUTH_WIDE, MOUTH_FULL, MOUTH_WIDE, MOUTH_MEDIUM, MOUTH_SMALL};
const int TALK_FRAMES_COUNT = 8;

// Кадры для "слушаю"
const byte* LISTEN_FRAMES[] = {LISTEN_1, LISTEN_2, LISTEN_3, LISTEN_2};
const int LISTEN_FRAMES_COUNT = 4;

// === СОСТОЯНИЯ ===
enum State {
  STATE_SLEEP,
  STATE_IDLE,
  STATE_LISTENING,
  STATE_TALKING
};

State currentState = STATE_SLEEP;
bool animating = false;         // автоматическая анимация разговора
bool listening = false;         // анимация "слушаю"
int animFrame = 0;
unsigned long lastAnimTime = 0;
unsigned long lastMeasureTime = 0;
unsigned long lastFarTime = 0;  // когда последний раз было далеко
bool wasClose = false;
bool wakeSent = false;
bool sleepSent = true;
String inputBuffer = "";

// === ФУНКЦИИ ===

void displayBitmap(const byte bitmap[8]) {
  for (int row = 0; row < 8; row++) {
    lc.setRow(0, row, bitmap[row]);
  }
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // таймаут 30мс
  if (duration == 0) return 999; // нет отражения
  return duration / 58; // перевод в см
}

void processCommand(String cmd) {
  cmd.trim();

  if (cmd == "M0") {
    animating = false;
    listening = false;
    displayBitmap(MOUTH_CLOSED);
  }
  else if (cmd == "M1") {
    animating = false;
    listening = false;
    displayBitmap(MOUTH_SMALL);
  }
  else if (cmd == "M2") {
    animating = false;
    listening = false;
    displayBitmap(MOUTH_MEDIUM);
  }
  else if (cmd == "M3") {
    animating = false;
    listening = false;
    displayBitmap(MOUTH_WIDE);
  }
  else if (cmd == "M4") {
    animating = false;
    listening = false;
    displayBitmap(MOUTH_FULL);
  }
  else if (cmd == "L1") {
    // Режим "слушаю"
    animating = false;
    listening = true;
    animFrame = 0;
    lastAnimTime = millis();
    currentState = STATE_LISTENING;
  }
  else if (cmd == "A1") {
    // Начать анимацию разговора
    listening = false;
    animating = true;
    animFrame = 0;
    lastAnimTime = millis();
    currentState = STATE_TALKING;
  }
  else if (cmd == "A0") {
    // Остановить анимацию
    animating = false;
    listening = false;
    displayBitmap(MOUTH_CLOSED);
    currentState = STATE_IDLE;
  }
  else if (cmd == "S1") {
    // Режим сна
    animating = false;
    listening = false;
    displayBitmap(SLEEP_FACE);
    currentState = STATE_SLEEP;
  }
  else if (cmd == "S0") {
    // Выйти из сна
    displayBitmap(MOUTH_CLOSED);
    currentState = STATE_IDLE;
  }
}

void setup() {
  Serial.begin(9600);

  // Инициализация MAX7219
  lc.shutdown(0, false);   // включить
  lc.setIntensity(0, 4);   // яркость 0-15
  lc.clearDisplay(0);

  // HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Начальное состояние — сон
  displayBitmap(SLEEP_FACE);
  currentState = STATE_SLEEP;

  lastFarTime = millis();
}

void loop() {
  unsigned long now = millis();

  // --- Чтение Serial ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      processCommand(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }

  // --- Замер расстояния ---
  if (now - lastMeasureTime >= MEASURE_INTERVAL) {
    lastMeasureTime = now;
    long dist = measureDistance();
    bool isClose = (dist < WAKE_DISTANCE_CM);

    if (isClose) {
      lastFarTime = now; // сбрасываем таймер "далеко"

      if (!wakeSent) {
        Serial.println("WAKE");
        wakeSent = true;
        sleepSent = false;

        if (currentState == STATE_SLEEP) {
          displayBitmap(MOUTH_CLOSED);
          currentState = STATE_IDLE;
        }
      }
    } else {
      // Объект далеко
      if (wakeSent && !sleepSent && (now - lastFarTime >= SLEEP_DELAY)) {
        Serial.println("SLEEP");
        sleepSent = true;
        wakeSent = false;

        animating = false;
        listening = false;
        displayBitmap(SLEEP_FACE);
        currentState = STATE_SLEEP;
      }
    }
  }

  // --- Анимация разговора ---
  if (animating && (now - lastAnimTime >= 120)) {
    lastAnimTime = now;
    displayBitmap(TALK_FRAMES[animFrame]);
    animFrame = (animFrame + 1) % TALK_FRAMES_COUNT;
  }

  // --- Анимация "слушаю" ---
  if (listening && (now - lastAnimTime >= 300)) {
    lastAnimTime = now;
    displayBitmap(LISTEN_FRAMES[animFrame]);
    animFrame = (animFrame + 1) % LISTEN_FRAMES_COUNT;
  }
}
