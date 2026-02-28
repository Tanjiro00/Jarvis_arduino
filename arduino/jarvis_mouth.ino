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
const unsigned long SLEEP_DELAY      = 10000;
const unsigned long MEASURE_INTERVAL = 200;
const unsigned long TALK_FRAME_MS    = 150;
const unsigned long LISTEN_FRAME_MS  = 400;

// === КАСТОМНЫЕ СИМВОЛЫ (5x8) ===
// Загружаются один раз в setup() — слоты 0..7
// Рот = 2 символа рядом: левый (чётный слот) + правый (нечётный слот)
//
// Слот 0-1: рот закрыт
// Слот 2-3: рот средне открыт
// Слот 4-5: рот широко открыт
// Слот 6-7: анимация "слушаю"

byte C0[8] = { // закрыт — левая
  0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000
};
byte C1[8] = { // закрыт — правая
  0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000
};
byte C2[8] = { // средне — левая
  0b00000, 0b01111, 0b10000, 0b10000, 0b10000, 0b01111, 0b00000, 0b00000
};
byte C3[8] = { // средне — правая
  0b00000, 0b11110, 0b00001, 0b00001, 0b00001, 0b11110, 0b00000, 0b00000
};
byte C4[8] = { // широко — левая
  0b00111, 0b01000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01000, 0b00111
};
byte C5[8] = { // широко — правая
  0b11100, 0b00010, 0b00001, 0b00001, 0b00001, 0b00001, 0b00010, 0b11100
};
byte C6[8] = { // слушаю — малый пульс — левая
  0b00000, 0b00000, 0b00100, 0b01110, 0b00100, 0b00000, 0b00000, 0b00000
};
byte C7[8] = { // слушаю — малый пульс — правая
  0b00000, 0b00000, 0b00100, 0b01110, 0b00100, 0b00000, 0b00000, 0b00000
};

// === КАДРЫ АНИМАЦИИ ===
// Каждый кадр = пара слотов: {левый, правый}
// Разговор: закрыт → средне → широко → средне → закрыт → ...
const byte TALK_L[] = {0, 2, 4, 2, 0};
const byte TALK_R[] = {1, 3, 5, 3, 1};
const int  TALK_LEN = 5;

// Слушаю: {закрыт, пульс, закрыт, пульс, ...}
const byte LISTEN_L[] = {0, 6};
const byte LISTEN_R[] = {1, 7};
const int  LISTEN_LEN = 2;

// === СОСТОЯНИЕ ===
bool animating = false;
bool listening = false;
int  animFrame = 0;
unsigned long lastAnimTime    = 0;
unsigned long lastMeasureTime = 0;
unsigned long lastFarTime     = 0;
bool wakeSent  = false;
bool sleepSent = true;
String inputBuffer = "";

// === ФУНКЦИИ ===

void drawMouth(byte slotL, byte slotR) {
  lcd.setCursor(7, 1);
  lcd.write(slotL);
  lcd.write(slotR);
}

void setStatus(const char* text) {
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print(text);
}

void showSleep() {
  lcd.setCursor(0, 0);
  lcd.print("     zzZ...     ");
  lcd.setCursor(0, 1);
  lcd.print("      ~_~       ");
}

void showIdle() {
  setStatus("    [ JARVIS ]  ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  drawMouth(0, 1);
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
    drawMouth(0, 1);
  } else if (cmd == "M1") {
    animating = false; listening = false;
    drawMouth(0, 1); // слегка = закрытый (нет отдельного слота)
  } else if (cmd == "M2") {
    animating = false; listening = false;
    drawMouth(2, 3);
  } else if (cmd == "M3" || cmd == "M4") {
    animating = false; listening = false;
    drawMouth(4, 5);
  } else if (cmd == "L1") {
    animating = false;
    listening = true;
    animFrame = 0;
    lastAnimTime = millis();
    setStatus("  [ LISTENING ] ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
  } else if (cmd == "A1") {
    listening = false;
    animating = true;
    animFrame = 0;
    lastAnimTime = millis();
    setStatus("  [ SPEAKING ]  ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
  } else if (cmd == "A0") {
    animating = false; listening = false;
    showIdle();
  } else if (cmd == "S1") {
    animating = false; listening = false;
    showSleep();
  } else if (cmd == "S0") {
    showIdle();
  }
}

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  // Загружаем все 8 кастомных символов ОДИН РАЗ
  lcd.createChar(0, C0);
  lcd.createChar(1, C1);
  lcd.createChar(2, C2);
  lcd.createChar(3, C3);
  lcd.createChar(4, C4);
  lcd.createChar(5, C5);
  lcd.createChar(6, C6);
  lcd.createChar(7, C7);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  showSleep();
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
    } else if (c != '\r') {
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
        showIdle();
      }
    } else {
      if (wakeSent && !sleepSent && (now - lastFarTime >= SLEEP_DELAY)) {
        Serial.println("SLEEP");
        sleepSent = true;
        wakeSent  = false;
        animating = false;
        listening = false;
        showSleep();
      }
    }
  }

  // --- Анимация разговора ---
  if (animating && (now - lastAnimTime >= TALK_FRAME_MS)) {
    lastAnimTime = now;
    drawMouth(TALK_L[animFrame], TALK_R[animFrame]);
    animFrame = (animFrame + 1) % TALK_LEN;
  }

  // --- Анимация "слушаю" ---
  if (listening && (now - lastAnimTime >= LISTEN_FRAME_MS)) {
    lastAnimTime = now;
    drawMouth(LISTEN_L[animFrame], LISTEN_R[animFrame]);
    animFrame = (animFrame + 1) % LISTEN_LEN;
  }
}
