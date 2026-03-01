#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// === HC-SR04 ===
const int TRIG_PIN = 7;
const int ECHO_PIN = 6;
const int WAKE_DIST = 80;
const unsigned long SLEEP_TIMEOUT = 12000;
const unsigned long MEASURE_MS    = 250;

// === ТАЙМИНГИ ===
const unsigned long TALK_MS   = 100;
const unsigned long LISTEN_MS = 500;
const unsigned long BLINK_MIN = 2500;
const unsigned long BLINK_MAX = 6000;
const unsigned long BLINK_DUR = 150;

// =====================================================
// КАСТОМНЫЕ СИМВОЛЫ — АНИМЕ ЛИЦО
//
// Каждый глаз = 2 символа (10x8 пикселей)
// Row 0: глаза на позициях 2-3 и 12-13
// Row 1: рот на позиции 7 (один символ)
//
// Слоты:
//   0 = глаз левая половина (открыт)
//   1 = глаз правая половина (открыт)
//   2 = глаз левая половина (закрыт)
//   3 = глаз правая половина (закрыт)
//   4 = рот: улыбка
//   5 = рот: маленький "о"
//   6 = рот: большой "О"
//   7 = рот: точки (слушает)
// =====================================================

// --- БОЛЬШИЕ АНИМЕ ГЛАЗА (10x8 на глаз) ---
// Круглая форма, огромный зрачок, блик в верху

byte EYE_OPEN_L[8] = {
  0b00111, // ..###
  0b01000, // .#...
  0b10001, // #...#  <- блик (свободные пиксели = свет)
  0b10011, // #..##
  0b10111, // #.###
  0b10011, // #..##
  0b01000, // .#...
  0b00111  // ..###
};

byte EYE_OPEN_R[8] = {
  0b11100, // ###..
  0b00010, // ...#.
  0b00001, // ....#  <- блик справа
  0b11001, // ##..#
  0b11101, // ###.#
  0b11001, // ##..#
  0b00010, // ...#.
  0b11100  // ###..
};

// Глаза закрыты — дуга (^_^)
byte EYE_SHUT_L[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b00011,
  0b00100,
  0b01000,
  0b00000,
  0b00000
};

byte EYE_SHUT_R[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b11000,
  0b00100,
  0b00010,
  0b00000,
  0b00000
};

// --- РОТ (одиночные символы) ---

// Улыбка :)
byte MOUTH_SMILE[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b10001,
  0b10001,
  0b01110,
  0b00000,
  0b00000
};

// Маленький "о"
byte MOUTH_SM[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b10001,
  0b10001,
  0b01110,
  0b00000,
  0b00000
};

// Большой "О"
byte MOUTH_BIG[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b01110,
  0b00000
};

// Точки "слушает"
byte MOUTH_DOT[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b10101,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

// === ГЛАЗА: динамические (для взгляда) ===
// Загружаются в слоты 0-1 когда нужно посмотреть в сторону

byte EYE_LOOK_L_L[8] = { // смотрит влево — левая половина
  0b00111, 0b01000, 0b10100, 0b10110, 0b10111, 0b10011, 0b01000, 0b00111
};
byte EYE_LOOK_L_R[8] = { // смотрит влево — правая половина
  0b11100, 0b00010, 0b00001, 0b10001, 0b11101, 0b11001, 0b00010, 0b11100
};

byte EYE_LOOK_R_L[8] = { // смотрит вправо — левая половина
  0b00111, 0b01000, 0b10001, 0b10001, 0b10111, 0b10011, 0b01000, 0b00111
};
byte EYE_LOOK_R_R[8] = { // смотрит вправо — правая половина
  0b11100, 0b00010, 0b00101, 0b01101, 0b11101, 0b11001, 0b00010, 0b11100
};

// === РАЗГОВОР ===
const byte TALK_MOUTH[] = {4, 5, 6, 5, 4, 5, 6, 6, 5, 4, 5, 6, 5, 4};
const int  TALK_LEN     = 14;

// === СОСТОЯНИЕ ===
enum State { STATE_SLEEP, STATE_IDLE, STATE_LISTENING, STATE_TALKING };
State currentState = STATE_SLEEP;

bool animating = false;
bool listening = false;
int  animFrame = 0;
unsigned long lastAnimTime = 0;

// HC-SR04
unsigned long lastMeasureTime = 0;
unsigned long lastFarTime = 0;
bool wakeSent  = false;
bool sleepSent = true;

// Моргание
unsigned long nextBlinkTime = 0;
bool blinking = false;
unsigned long blinkStartTime = 0;

// Взгляд
unsigned long nextLookTime = 0;
bool looking = false;
unsigned long lookStartTime = 0;

int listenDotPhase = 0;

String inputBuffer = "";

// === ФУНКЦИИ ===

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  return (dur == 0) ? 999 : dur / 58;
}

void scheduleNextBlink() {
  nextBlinkTime = millis() + random(BLINK_MIN, BLINK_MAX);
}

void scheduleNextLook() {
  nextLookTime = millis() + random(5000, 12000);
}

void loadDefaultEyes() {
  lcd.createChar(0, EYE_OPEN_L);
  lcd.createChar(1, EYE_OPEN_R);
}

void drawEyes(byte slotL, byte slotR) {
  // Левый глаз: cols 2-3
  lcd.setCursor(2, 0);
  lcd.write(slotL);
  lcd.write(slotR);
  // Правый глаз: cols 12-13
  lcd.setCursor(12, 0);
  lcd.write(slotL);
  lcd.write(slotR);
}

void drawMouth(byte slot) {
  lcd.setCursor(7, 1);
  lcd.write(slot);
}

void drawFace() {
  lcd.clear();
  loadDefaultEyes();
  drawEyes(0, 1);
  drawMouth(4);  // улыбка
  scheduleNextBlink();
  scheduleNextLook();
}

// === BOOT ===
void bootAnimation() {
  lcd.clear();
  lcd.backlight();
  delay(300);

  // Глаза появляются: сначала закрытые
  drawEyes(2, 3);
  delay(500);

  // Открываются
  loadDefaultEyes();
  drawEyes(0, 1);
  delay(200);

  // Быстрое моргание (проснулись)
  drawEyes(2, 3);
  delay(100);
  drawEyes(0, 1);
  delay(150);
  drawEyes(2, 3);
  delay(100);
  drawEyes(0, 1);
  delay(400);

  // Улыбка
  drawMouth(4);
  delay(300);

  // Имя бегущей строкой внизу
  const char* name = "  ZHOPA ONLINE  ";
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i, 1);
    lcd.print(name[i]);
    delay(40);
  }
  delay(900);

  // Переход к нормальному лицу
  drawFace();
}

// === ЭКРАНЫ ===

void showSleep() {
  lcd.clear();
  // Спящее лицо: закрытые глаза + zzZ
  drawEyes(2, 3);
  lcd.setCursor(6, 1);
  lcd.print("z Z z");
  delay(300);
  lcd.noBacklight();
  currentState = STATE_SLEEP;
}

void showIdle() {
  lcd.backlight();
  drawFace();
  currentState = STATE_IDLE;
}

void showListening() {
  loadDefaultEyes();
  drawEyes(0, 1);
  lcd.setCursor(0, 1);
  lcd.print("                ");
  drawMouth(7);
  listenDotPhase = 0;
  currentState = STATE_LISTENING;
}

void showSpeaking() {
  loadDefaultEyes();
  drawEyes(0, 1);
  currentState = STATE_TALKING;
}

// === ОБРАБОТКА КОМАНД ===
void processCommand(String cmd) {
  cmd.trim();

  if (cmd == "M0") {
    animating = false; listening = false;
    drawMouth(4);
  } else if (cmd == "M1") {
    animating = false; listening = false;
    drawMouth(5);
  } else if (cmd == "M2" || cmd == "M3" || cmd == "M4") {
    animating = false; listening = false;
    drawMouth(6);
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
    animating = false;
    listening = false;
    // Засыпание: глаза медленно закрываются
    drawMouth(4);
    delay(300);
    drawEyes(2, 3);
    delay(500);
    showSleep();
  } else if (cmd == "S0") {
    lcd.backlight();
    bootAnimation();
  }
}

// === SETUP ===
void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A0));

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, EYE_OPEN_L);
  lcd.createChar(1, EYE_OPEN_R);
  lcd.createChar(2, EYE_SHUT_L);
  lcd.createChar(3, EYE_SHUT_R);
  lcd.createChar(4, MOUTH_SMILE);
  lcd.createChar(5, MOUTH_SM);
  lcd.createChar(6, MOUTH_BIG);
  lcd.createChar(7, MOUTH_DOT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  showSleep();
  lastFarTime = millis();
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

  // --- HC-SR04 ---
  if (now - lastMeasureTime >= MEASURE_MS) {
    lastMeasureTime = now;
    long dist = measureDistance();

    if (dist < WAKE_DIST) {
      lastFarTime = now;
      if (!wakeSent) {
        Serial.println("WAKE");
        wakeSent = true;
        sleepSent = false;
        if (currentState == STATE_SLEEP) {
          lcd.backlight();
          bootAnimation();
        }
      }
    } else {
      if (wakeSent && !sleepSent && (now - lastFarTime >= SLEEP_TIMEOUT)) {
        Serial.println("SLEEP");
        sleepSent = true;
        wakeSent = false;
        animating = false;
        listening = false;
        drawEyes(2, 3);
        delay(400);
        showSleep();
      }
    }
  }

  // --- МОРГАНИЕ ---
  if ((currentState == STATE_IDLE || currentState == STATE_TALKING) && !blinking && !looking) {
    if (now >= nextBlinkTime) {
      blinking = true;
      blinkStartTime = now;
      drawEyes(2, 3);
    }
  }
  if (blinking && (now - blinkStartTime >= BLINK_DUR)) {
    blinking = false;
    loadDefaultEyes();
    drawEyes(0, 1);
    scheduleNextBlink();
  }

  // --- ВЗГЛЯД В СТОРОНЫ (только idle) ---
  if (currentState == STATE_IDLE && !blinking && !looking) {
    if (now >= nextLookTime) {
      looking = true;
      lookStartTime = now;
      byte dir = random(0, 2);
      if (dir == 0) {
        lcd.createChar(0, EYE_LOOK_L_L);
        lcd.createChar(1, EYE_LOOK_L_R);
      } else {
        lcd.createChar(0, EYE_LOOK_R_L);
        lcd.createChar(1, EYE_LOOK_R_R);
      }
      drawEyes(0, 1);
    }
  }
  if (looking && (now - lookStartTime >= 700)) {
    looking = false;
    loadDefaultEyes();
    drawEyes(0, 1);
    scheduleNextLook();
  }

  // --- РАЗГОВОР ---
  if (animating && (now - lastAnimTime >= TALK_MS + random(0, 40))) {
    lastAnimTime = now;
    drawMouth(TALK_MOUTH[animFrame]);
    animFrame = (animFrame + 1) % TALK_LEN;
  }

  // --- СЛУШАЮ: пульсирующие точки ---
  if (listening && (now - lastAnimTime >= LISTEN_MS)) {
    lastAnimTime = now;
    lcd.setCursor(5, 1);
    lcd.print("      ");
    lcd.setCursor(5, 1);
    listenDotPhase = (listenDotPhase + 1) % 4;
    for (int i = 0; i <= listenDotPhase; i++) {
      lcd.write(byte(7));
      if (i < listenDotPhase) lcd.print(" ");
    }
  }
}
