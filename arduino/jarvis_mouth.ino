#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// === LCD I2C ===
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === ТАЙМИНГИ ===
const unsigned long TALK_FRAME_MS   = 90;
const unsigned long LISTEN_FRAME_MS = 180;
const unsigned long IDLE_ANIM_MS    = 120;   // скорость idle-анимации
const unsigned long IDLE_PAUSE_MS   = 4000;  // пауза между вдохами

// === КАСТОМНЫЕ СИМВОЛЫ (8 слотов) ===

// 0-1: рот закрыт — улыбка
byte SHUT_L[8]  = {
  0b00000, 0b00000, 0b00000, 0b00000, 0b01111, 0b00111, 0b00000, 0b00000
};
byte SHUT_R[8]  = {
  0b00000, 0b00000, 0b00000, 0b00000, 0b11110, 0b11100, 0b00000, 0b00000
};

// 2-3: рот приоткрыт
byte SM_L[8]    = {
  0b00000, 0b00000, 0b00111, 0b01000, 0b01000, 0b00111, 0b00000, 0b00000
};
byte SM_R[8]    = {
  0b00000, 0b00000, 0b11100, 0b00010, 0b00010, 0b11100, 0b00000, 0b00000
};

// 4-5: рот широко открыт
byte BIG_L[8]   = {
  0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b00100, 0b00010, 0b00001
};
byte BIG_R[8]   = {
  0b10000, 0b01000, 0b00100, 0b00010, 0b00010, 0b00100, 0b01000, 0b10000
};

// 6: звуковая волна фаза A
byte WAVE_A[8]  = {
  0b00000, 0b00100, 0b01010, 0b10001, 0b01010, 0b00100, 0b00000, 0b00000
};

// 7: звуковая волна фаза B
byte WAVE_B[8]  = {
  0b00000, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b00000, 0b00000
};

// === АНИМАЦИЯ РАЗГОВОРА ===
// Имитация живой речи: неравномерный ритм
const byte TALK_L[] = {0, 2, 4, 4, 2, 0, 0, 2, 4, 2, 4, 4, 2, 0, 0, 2};
const byte TALK_R[] = {1, 3, 5, 5, 3, 1, 1, 3, 5, 3, 5, 5, 3, 1, 1, 3};
const int  TALK_LEN = 16;

// === СОСТОЯНИЯ ===
enum State { STATE_BOOT, STATE_IDLE, STATE_LISTENING, STATE_TALKING };
State currentState = STATE_BOOT;

bool animating = false;
bool listening = false;
int  animFrame = 0;
unsigned long lastAnimTime = 0;

// Idle "дыхание"
int  idlePhase = 0;       // 0=пауза, 1=вдох, 2=выдох
int  idleStep  = 0;
unsigned long lastIdleTime = 0;

// Idle: кадры дыхания рта (вдох: shut→sm→shut, пауза)
const byte BREATHE_L[] = {0, 2, 2, 0};
const byte BREATHE_R[] = {1, 3, 3, 1};
const int  BREATHE_LEN = 4;

String inputBuffer = "";

// === BOOT: кинематографическая загрузка ===
void bootAnimation() {
  lcd.clear();
  delay(300);

  // Фаза 1: горизонтальная линия "сканирования"
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i, 0);
    lcd.print((char)0xFF);
    lcd.setCursor(i, 1);
    lcd.print((char)0xFF);
    delay(25);
    if (i > 0) {
      lcd.setCursor(i - 1, 0);
      lcd.print(" ");
      lcd.setCursor(i - 1, 1);
      lcd.print(" ");
    }
  }
  lcd.setCursor(15, 0); lcd.print(" ");
  lcd.setCursor(15, 1); lcd.print(" ");
  delay(200);

  // Фаза 2: имя появляется из центра
  const char* name = "J.O.P.A.";
  int nameLen = 8;
  int center = 8;
  for (int half = 1; half <= (nameLen + 1) / 2; half++) {
    int left  = center - half;
    int right = center + half - 1;
    if (left >= 0 && (half - 1) < nameLen)
      { lcd.setCursor(left + (16 - nameLen) / 2, 0); }
    // Выводим посимвольно от центра
    lcd.setCursor((16 - nameLen) / 2, 0);
    for (int j = 0; j < half * 2 && j < nameLen; j++) {
      lcd.print(name[j]);
    }
    delay(100);
  }
  delay(400);

  // Фаза 3: прогресс-бар с процентами
  lcd.setCursor(0, 1);
  lcd.print("[              ]");
  for (int i = 1; i <= 14; i++) {
    lcd.setCursor(i, 1);
    lcd.print((char)0xFF);
    // Проценты
    int pct = (i * 100) / 14;
    lcd.setCursor(6, 0);  // перезаписываем центр
    lcd.print("    ");     // очистить
    lcd.setCursor(6, 0);
    if (pct < 100) lcd.print(" ");
    lcd.print(pct);
    lcd.print("%");
    delay(50 + random(30));  // случайная задержка = реалистичность
  }
  delay(200);

  // Фаза 4: имя + ONLINE
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("J.O.P.A.");

  // Мигание статуса
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(3, 1);
    lcd.print("[ ONLINE ]");
    delay(200);
    lcd.setCursor(3, 1);
    lcd.print("          ");
    delay(120);
  }
  lcd.setCursor(3, 1);
  lcd.print("[ ONLINE ]");
  delay(600);
}

// === ОТРИСОВКА ===

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

// === ЭКРАНЫ СОСТОЯНИЙ ===

void showIdle() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("<< J.O.P.A. >>");
  drawMouth(0, 1);
  idlePhase = 0;
  idleStep = 0;
  lastIdleTime = millis();
  currentState = STATE_IDLE;
}

void showListening() {
  lcd.clear();
  // Верхняя строка: уши
  lcd.setCursor(0, 0);
  lcd.print("((  Listening  ))");
  currentState = STATE_LISTENING;
}

void showSpeaking() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(">>  Speaking  <<");
  currentState = STATE_TALKING;
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
  } else if (cmd == "A1") {
    listening = false;
    animating = true;
    animFrame = 0;
    lastAnimTime = millis();
    showSpeaking();
  } else if (cmd == "A0") {
    animating = false;
    listening = false;
    showIdle();
  } else if (cmd == "S1") {
    // Засыпание
    animating = false;
    listening = false;
    lcd.clear();
    centerText(0, "Goodbye.");
    delay(600);
    // Затухание: убираем текст постепенно
    for (int i = 15; i >= 0; i--) {
      lcd.setCursor(i, 0); lcd.print(" ");
      lcd.setCursor(i, 1); lcd.print(" ");
      delay(40);
    }
    lcd.noBacklight();
  } else if (cmd == "S0") {
    lcd.backlight();
    bootAnimation();
    showIdle();
  }
}

// === SETUP ===
void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A0));

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, SHUT_L);
  lcd.createChar(1, SHUT_R);
  lcd.createChar(2, SM_L);
  lcd.createChar(3, SM_R);
  lcd.createChar(4, BIG_L);
  lcd.createChar(5, BIG_R);
  lcd.createChar(6, WAVE_A);
  lcd.createChar(7, WAVE_B);

  bootAnimation();
  showIdle();
}

// === LOOP ===
void loop() {
  unsigned long now = millis();

  // --- Serial ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      processCommand(inputBuffer);
      inputBuffer = "";
    } else if (c != '\r') {
      inputBuffer += c;
    }
  }

  // --- IDLE: дыхание ---
  if (currentState == STATE_IDLE) {
    if (idlePhase == 0) {
      // Пауза между вдохами
      if (now - lastIdleTime >= IDLE_PAUSE_MS) {
        idlePhase = 1;
        idleStep = 0;
        lastIdleTime = now;
      }
    } else if (idlePhase == 1) {
      // Анимация дыхания
      if (now - lastIdleTime >= IDLE_ANIM_MS) {
        lastIdleTime = now;
        drawMouth(BREATHE_L[idleStep], BREATHE_R[idleStep]);
        idleStep++;
        if (idleStep >= BREATHE_LEN) {
          idlePhase = 0;
          lastIdleTime = now;
        }
      }
    }
  }

  // --- Разговор: живая речь ---
  if (animating && (now - lastAnimTime >= TALK_FRAME_MS)) {
    // Случайная вариация скорости = более живо
    lastAnimTime = now - random(20);
    drawMouth(TALK_L[animFrame], TALK_R[animFrame]);
    animFrame = (animFrame + 1) % TALK_LEN;
  }

  // --- Слушаю: звуковые волны ---
  if (listening && (now - lastAnimTime >= LISTEN_FRAME_MS)) {
    lastAnimTime = now;
    clearRow(1);
    // Волны разбегаются от центра
    int center = 7;
    int spread = (animFrame % 4);

    for (int i = -spread; i <= spread; i++) {
      int col = center + i;
      if (col >= 0 && col < 16) {
        lcd.setCursor(col, 1);
        if (abs(i) == spread) {
          lcd.write(byte(animFrame % 2 == 0 ? 6 : 7));
        } else if (abs(i) > 0) {
          lcd.write(byte(animFrame % 2 == 0 ? 7 : 6));
        } else {
          lcd.write(byte(6));
        }
      }
    }
    animFrame++;
  }
}
