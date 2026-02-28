#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// === LCD I2C ===
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === НАСТРОЙКИ АНИМАЦИИ ===
const unsigned long TALK_FRAME_MS    = 100;
const unsigned long LISTEN_FRAME_MS  = 250;
const unsigned long IDLE_FRAME_MS    = 3000;  // моргание в idle
const unsigned long BOOT_STEP_MS     = 80;

// === КАСТОМНЫЕ СИМВОЛЫ ===
// Слот 0-1: рот закрыт (тонкая линия)
byte MOUTH_SHUT_L[8] = {
  0b00000, 0b00000, 0b00000, 0b01111, 0b00000, 0b00000, 0b00000, 0b00000
};
byte MOUTH_SHUT_R[8] = {
  0b00000, 0b00000, 0b00000, 0b11110, 0b00000, 0b00000, 0b00000, 0b00000
};

// Слот 2-3: рот приоткрыт (маленький овал)
byte MOUTH_SM_L[8] = {
  0b00000, 0b00000, 0b00111, 0b01000, 0b00111, 0b00000, 0b00000, 0b00000
};
byte MOUTH_SM_R[8] = {
  0b00000, 0b00000, 0b11100, 0b00010, 0b11100, 0b00000, 0b00000, 0b00000
};

// Слот 4-5: рот широко (большой овал)
byte MOUTH_BIG_L[8] = {
  0b00001, 0b00110, 0b01000, 0b10000, 0b10000, 0b01000, 0b00110, 0b00001
};
byte MOUTH_BIG_R[8] = {
  0b10000, 0b01100, 0b00010, 0b00001, 0b00001, 0b00010, 0b01100, 0b10000
};

// Слот 6: пульс "слушаю" (эквалайзер)
byte LISTEN_EQ[8] = {
  0b00000, 0b00100, 0b00100, 0b10101, 0b10101, 0b11111, 0b11111, 0b00000
};

// Слот 7: пульс "слушаю" фаза 2
byte LISTEN_EQ2[8] = {
  0b00000, 0b01010, 0b11111, 0b11111, 0b01110, 0b00100, 0b00000, 0b00000
};

// === АНИМАЦИЯ РАЗГОВОРА ===
// Плавный цикл: shut → small → big → small → shut
const byte TALK_L[] = {0, 2, 4, 2, 0, 2, 4, 4, 2, 0};
const byte TALK_R[] = {1, 3, 5, 3, 1, 3, 5, 5, 3, 1};
const int  TALK_LEN = 10;

// === СОСТОЯНИЯ ===
enum State { STATE_BOOT, STATE_IDLE, STATE_LISTENING, STATE_TALKING };
State currentState = STATE_BOOT;

bool animating = false;
bool listening = false;
int  animFrame = 0;
unsigned long lastAnimTime = 0;
unsigned long lastIdleBlink = 0;
bool idleBlinkOn = false;

// Бегущая строка
int  scrollPos = 0;
unsigned long lastScrollTime = 0;
const unsigned long SCROLL_MS = 200;

String inputBuffer = "";

// === BOOT АНИМАЦИЯ ===
void bootAnimation() {
  lcd.clear();

  // Строка 1: побуквенное появление "J.A.R.V.I.S."
  const char* name = "J.A.R.V.I.S.";
  int startCol = (16 - 13) / 2; // центрирование
  lcd.setCursor(startCol, 0);
  for (int i = 0; i < 13; i++) {
    lcd.print(name[i]);
    delay(BOOT_STEP_MS);
  }

  delay(300);

  // Строка 2: прогресс-бар загрузки
  lcd.setCursor(0, 1);
  lcd.print("[");
  lcd.setCursor(15, 1);
  lcd.print("]");
  for (int i = 1; i <= 14; i++) {
    lcd.setCursor(i, 1);
    lcd.print((char)0xFF); // заполненный блок
    delay(40);
  }

  delay(300);

  // Мигание "ONLINE"
  lcd.setCursor(0, 1);
  lcd.print("                ");
  for (int blink = 0; blink < 3; blink++) {
    lcd.setCursor(4, 1);
    lcd.print("[ ONLINE ]");
    delay(250);
    lcd.setCursor(4, 1);
    lcd.print("          ");
    delay(150);
  }
  lcd.setCursor(4, 1);
  lcd.print("[ ONLINE ]");
  delay(500);
}

// === ФУНКЦИИ ОТРИСОВКИ ===

void drawMouth(byte slotL, byte slotR) {
  lcd.setCursor(7, 1);
  lcd.write(slotL);
  lcd.write(slotR);
}

void clearRow(int row) {
  lcd.setCursor(0, row);
  lcd.print("                ");
}

void centerText(int row, const char* text) {
  int len = strlen(text);
  int col = (16 - len) / 2;
  if (col < 0) col = 0;
  clearRow(row);
  lcd.setCursor(col, row);
  lcd.print(text);
}

// === IDLE: красивый экран ожидания ===
void showIdle() {
  centerText(0, "< J.A.R.V.I.S >");
  clearRow(1);
  drawMouth(0, 1); // закрытый рот
  idleBlinkOn = false;
  lastIdleBlink = millis();
}

// === СЛУШАЮ: эквалайзер ===
void showListening() {
  centerText(0, "Listening...");
  clearRow(1);
}

// === ГОВОРЮ ===
void showSpeaking() {
  centerText(0, "Speaking...");
  clearRow(1);
}

// === ОБРАБОТКА КОМАНД ===
void processCommand(String cmd) {
  cmd.trim();

  if (cmd == "M0") {
    animating = false; listening = false;
    drawMouth(0, 1);
  } else if (cmd == "M1") {
    animating = false; listening = false;
    drawMouth(2, 3);
  } else if (cmd == "M2" || cmd == "M3" || cmd == "M4") {
    animating = false; listening = false;
    drawMouth(4, 5);
  } else if (cmd == "L1") {
    animating = false;
    listening = true;
    animFrame = 0;
    lastAnimTime = millis();
    showListening();
    currentState = STATE_LISTENING;
  } else if (cmd == "A1") {
    listening = false;
    animating = true;
    animFrame = 0;
    lastAnimTime = millis();
    showSpeaking();
    currentState = STATE_TALKING;
  } else if (cmd == "A0") {
    animating = false;
    listening = false;
    showIdle();
    currentState = STATE_IDLE;
  } else if (cmd == "S1") {
    // Красивое засыпание
    animating = false;
    listening = false;
    clearRow(1);
    centerText(0, "Shutting down...");
    delay(800);
    lcd.clear();
    lcd.noBacklight();
  } else if (cmd == "S0") {
    // Красивое пробуждение
    lcd.backlight();
    bootAnimation();
    showIdle();
    currentState = STATE_IDLE;
  }
}

// === SETUP ===
void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  // Загружаем все кастомные символы
  lcd.createChar(0, MOUTH_SHUT_L);
  lcd.createChar(1, MOUTH_SHUT_R);
  lcd.createChar(2, MOUTH_SM_L);
  lcd.createChar(3, MOUTH_SM_R);
  lcd.createChar(4, MOUTH_BIG_L);
  lcd.createChar(5, MOUTH_BIG_R);
  lcd.createChar(6, LISTEN_EQ);
  lcd.createChar(7, LISTEN_EQ2);

  // Красивый запуск
  bootAnimation();
  showIdle();
  currentState = STATE_IDLE;
}

// === LOOP ===
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

  // --- IDLE: "моргание" рта ---
  if (currentState == STATE_IDLE && (now - lastIdleBlink >= IDLE_FRAME_MS)) {
    lastIdleBlink = now;
    if (idleBlinkOn) {
      drawMouth(0, 1); // закрытый
    } else {
      drawMouth(2, 3); // приоткрыть на секунду
    }
    idleBlinkOn = !idleBlinkOn;
  }

  // --- Анимация разговора ---
  if (animating && (now - lastAnimTime >= TALK_FRAME_MS)) {
    lastAnimTime = now;
    drawMouth(TALK_L[animFrame], TALK_R[animFrame]);
    animFrame = (animFrame + 1) % TALK_LEN;
  }

  // --- Анимация "слушаю": эквалайзер ---
  if (listening && (now - lastAnimTime >= LISTEN_FRAME_MS)) {
    lastAnimTime = now;
    lcd.setCursor(4, 1);
    if (animFrame % 2 == 0) {
      lcd.write(byte(6)); lcd.print(" ");
      lcd.write(byte(7)); lcd.print(" ");
      lcd.write(byte(6)); lcd.print(" ");
      lcd.write(byte(7)); lcd.print(" ");
    } else {
      lcd.write(byte(7)); lcd.print(" ");
      lcd.write(byte(6)); lcd.print(" ");
      lcd.write(byte(7)); lcd.print(" ");
      lcd.write(byte(6)); lcd.print(" ");
    }
    animFrame++;
  }
}
