#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// === LCD I2C ===
// Адрес 0x27 (чаще всего). Если не работает — попробуй 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === HC-SR04 ===
const int TRIG_PIN = 7;
const int ECHO_PIN = 6;

// === НАСТРОЙКИ ===
const int    WAKE_DISTANCE_CM = 100;
const unsigned long SLEEP_DELAY      = 10000; // 10 сек перед сном
const unsigned long MEASURE_INTERVAL = 200;   // замер каждые 200 мс
const unsigned long TALK_FRAME_MS    = 130;   // скорость анимации разговора
const unsigned long LISTEN_FRAME_MS  = 350;   // скорость анимации "слушаю"

// === КАСТОМНЫЕ СИМВОЛЫ (5x8 пикселей) ===
// Рот собирается из 2 символов рядом (col 7 и col 8, row 1)
// Левая половина — char 0, правая — char 1

// M0: закрытый рот — горизонтальная линия
byte CLOSED_L[8] = {
  0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000
};
byte CLOSED_R[8] = {
  0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000
};

// M1: слегка приоткрыт
byte SMALL_L[8] = {
  0b00000, 0b00000, 0b01111, 0b10000, 0b01111, 0b00000, 0b00000, 0b00000
};
byte SMALL_R[8] = {
  0b00000, 0b00000, 0b11110, 0b00001, 0b11110, 0b00000, 0b00000, 0b00000
};

// M2: средне открыт
byte MEDIUM_L[8] = {
  0b00000, 0b01111, 0b10000, 0b10000, 0b10000, 0b01111, 0b00000, 0b00000
};
byte MEDIUM_R[8] = {
  0b00000, 0b11110, 0b00001, 0b00001, 0b00001, 0b11110, 0b00000, 0b00000
};

// M3: широко открыт
byte WIDE_L[8] = {
  0b00111, 0b01000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01000, 0b00111
};
byte WIDE_R[8] = {
  0b11100, 0b00010, 0b00001, 0b00001, 0b00001, 0b00001, 0b00010, 0b11100
};

// M4: очень широко (с зубами)
byte FULL_L[8] = {
  0b00111, 0b01000, 0b10101, 0b10000, 0b10000, 0b10000, 0b01000, 0b00111
};
byte FULL_R[8] = {
  0b11100, 0b00010, 0b10101, 0b00001, 0b00001, 0b00001, 0b00010, 0b11100
};

// L: слушаю — "волна" (3 кадра, только левый символ меняется)
byte LISTEN_A_L[8] = { // маленький пульс
  0b00000, 0b00000, 0b00100, 0b01110, 0b00100, 0b00000, 0b00000, 0b00000
};
byte LISTEN_A_R[8] = {
  0b00000, 0b00000, 0b00100, 0b01110, 0b00100, 0b00000, 0b00000, 0b00000
};
byte LISTEN_B_L[8] = { // средний пульс
  0b00000, 0b00100, 0b01110, 0b11111, 0b01110, 0b00100, 0b00000, 0b00000
};
byte LISTEN_B_R[8] = {
  0b00000, 0b00100, 0b01110, 0b11111, 0b01110, 0b00100, 0b00000, 0b00000
};

// === ПОСЛЕДОВАТЕЛЬНОСТИ АНИМАЦИЙ ===
// Разговор: чередуем M0→M1→M2→M3→M2→M1→M0→...
const int TALK_SEQ[]  = {0, 1, 2, 3, 4, 3, 2, 1};
const int TALK_LEN    = 8;

// Слушаю: A→B→A→B→...
const int LISTEN_SEQ[] = {5, 6};
const int LISTEN_LEN   = 2;

// === СОСТОЯНИЯ ===
enum State { STATE_SLEEP, STATE_IDLE, STATE_LISTENING, STATE_TALKING };
State currentState = STATE_SLEEP;

bool animating  = false;
bool listening  = false;
int  animFrame  = 0;
unsigned long lastAnimTime    = 0;
unsigned long lastMeasureTime = 0;
unsigned long lastFarTime     = 0;
bool wakeSent   = false;
bool sleepSent  = true;
String inputBuffer = "";

// === ФУНКЦИИ ===

// Загрузить символы в LCD-CGRAM и нарисовать рот
void setMouth(byte* leftChar, byte* rightChar) {
  lcd.createChar(0, leftChar);
  lcd.createChar(1, rightChar);
  lcd.setCursor(7, 1);
  lcd.write(byte(0));
  lcd.write(byte(1));
}

void showFrame(int frameIndex) {
  switch (frameIndex) {
    case 0: setMouth(CLOSED_L,   CLOSED_R);  break;
    case 1: setMouth(SMALL_L,    SMALL_R);   break;
    case 2: setMouth(MEDIUM_L,   MEDIUM_R);  break;
    case 3: setMouth(WIDE_L,     WIDE_R);    break;
    case 4: setMouth(FULL_L,     FULL_R);    break;
    case 5: setMouth(LISTEN_A_L, LISTEN_A_R);break;
    case 6: setMouth(LISTEN_B_L, LISTEN_B_R);break;
  }
}

void setStatus(const char* text) {
  lcd.setCursor(0, 0);
  lcd.print("                "); // очистить строку
  lcd.setCursor(0, 0);
  lcd.print(text);
}

void showSleep() {
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("zzZ...");
  lcd.setCursor(7, 1);
  lcd.print(" ~_~");
}

void showIdle() {
  lcd.clear();
  setStatus("    [ JARVIS ]  ");
  showFrame(0); // закрытый рот
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  return (dur == 0) ? 999 : dur / 58;
}

void processCommand(String cmd) {
  cmd.trim();

  if (cmd == "M0") {
    animating = false; listening = false;
    showFrame(0);
  } else if (cmd == "M1") {
    animating = false; listening = false;
    showFrame(1);
  } else if (cmd == "M2") {
    animating = false; listening = false;
    showFrame(2);
  } else if (cmd == "M3") {
    animating = false; listening = false;
    showFrame(3);
  } else if (cmd == "M4") {
    animating = false; listening = false;
    showFrame(4);
  } else if (cmd == "L1") {
    animating = false;
    listening = true;
    animFrame = 0;
    lastAnimTime = millis();
    setStatus("   [ LISTENING ]");
    currentState = STATE_LISTENING;
  } else if (cmd == "A1") {
    listening = false;
    animating = true;
    animFrame = 0;
    lastAnimTime = millis();
    setStatus("   [ SPEAKING ] ");
    currentState = STATE_TALKING;
  } else if (cmd == "A0") {
    animating = false; listening = false;
    setStatus("    [ JARVIS ]  ");
    showFrame(0);
    currentState = STATE_IDLE;
  } else if (cmd == "S1") {
    animating = false; listening = false;
    showSleep();
    currentState = STATE_SLEEP;
  } else if (cmd == "S0") {
    showIdle();
    currentState = STATE_IDLE;
  }
}

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  showSleep();
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

    if (dist < WAKE_DISTANCE_CM) {
      lastFarTime = now;
      if (!wakeSent) {
        Serial.println("WAKE");
        wakeSent  = true;
        sleepSent = false;
        if (currentState == STATE_SLEEP) showIdle();
        currentState = STATE_IDLE;
      }
    } else {
      if (wakeSent && !sleepSent && (now - lastFarTime >= SLEEP_DELAY)) {
        Serial.println("SLEEP");
        sleepSent = true;
        wakeSent  = false;
        animating = false;
        listening = false;
        showSleep();
        currentState = STATE_SLEEP;
      }
    }
  }

  // --- Анимация разговора ---
  if (animating && (now - lastAnimTime >= TALK_FRAME_MS)) {
    lastAnimTime = now;
    showFrame(TALK_SEQ[animFrame]);
    animFrame = (animFrame + 1) % TALK_LEN;
  }

  // --- Анимация "слушаю" ---
  if (listening && (now - lastAnimTime >= LISTEN_FRAME_MS)) {
    lastAnimTime = now;
    showFrame(LISTEN_SEQ[animFrame]);
    animFrame = (animFrame + 1) % LISTEN_LEN;
  }
}
