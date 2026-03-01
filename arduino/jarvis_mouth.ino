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

// === БУФЕР Serial (защита от переполнения) ===
const int MAX_INPUT_LEN = 16;

// === HC-SR04 ФИЛЬТРАЦИЯ (медианный фильтр из 3) ===
long distHistory[3] = {999, 999, 999};
int distIdx = 0;

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

// --- БОЛЬШИЕ АНИМЕ ГЛАЗА ---
byte EYE_OPEN_L[8] = {
  0b00111,
  0b01000,
  0b10001,
  0b10011,
  0b10111,
  0b10011,
  0b01000,
  0b00111
};

byte EYE_OPEN_R[8] = {
  0b11100,
  0b00010,
  0b00001,
  0b11001,
  0b11101,
  0b11001,
  0b00010,
  0b11100
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

// Глаза полузакрыты (для плавного перехода)
byte EYE_HALF_L[8] = {
  0b00000,
  0b00000,
  0b00111,
  0b01011,
  0b10111,
  0b10011,
  0b01000,
  0b00111
};

byte EYE_HALF_R[8] = {
  0b00000,
  0b00000,
  0b11100,
  0b11010,
  0b11101,
  0b11001,
  0b00010,
  0b11100
};

// --- ЭМОЦИОНАЛЬНЫЕ ГЛАЗА ---

// Удивление — большие круглые (E1)
byte EYE_SURPRISE_L[8] = {
  0b00111,
  0b01000,
  0b10000,
  0b10010,
  0b10111,
  0b10010,
  0b01000,
  0b00111
};
byte EYE_SURPRISE_R[8] = {
  0b11100,
  0b00010,
  0b00001,
  0b01001,
  0b11101,
  0b01001,
  0b00010,
  0b11100
};

// Злость/дерзость — сужены сверху (E2)
byte EYE_ANGRY_L[8] = {
  0b00000,
  0b00000,
  0b00111,
  0b01011,
  0b10111,
  0b10011,
  0b01000,
  0b00111
};
byte EYE_ANGRY_R[8] = {
  0b00000,
  0b00000,
  0b11100,
  0b11010,
  0b11101,
  0b11001,
  0b00010,
  0b11100
};

// --- РОТ ---

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

byte EYE_LOOK_L_L[8] = {
  0b00111, 0b01000, 0b10100, 0b10110, 0b10111, 0b10011, 0b01000, 0b00111
};
byte EYE_LOOK_L_R[8] = {
  0b11100, 0b00010, 0b00001, 0b10001, 0b11101, 0b11001, 0b00010, 0b11100
};

byte EYE_LOOK_R_L[8] = {
  0b00111, 0b01000, 0b10001, 0b10001, 0b10111, 0b10011, 0b01000, 0b00111
};
byte EYE_LOOK_R_R[8] = {
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

// Эмоция (таймер возврата)
bool emotionActive = false;
unsigned long emotionStartTime = 0;
const unsigned long EMOTION_DUR = 2500;

// Плавное засыпание (state machine вместо delay)
enum SleepPhase { SLEEP_NONE, SLEEP_MOUTH, SLEEP_HALF, SLEEP_SHUT, SLEEP_DONE };
SleepPhase sleepPhase = SLEEP_NONE;
unsigned long sleepPhaseTime = 0;

// Плавное пробуждение
enum WakePhase { WAKE_NONE, WAKE_HALF, WAKE_OPEN, WAKE_BLINK1, WAKE_OPEN2, WAKE_BLINK2, WAKE_FINAL };
WakePhase wakePhase = WAKE_NONE;
unsigned long wakePhaseTime = 0;

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

long filteredDistance() {
  // Медианный фильтр из 3 значений
  long a = distHistory[0], b = distHistory[1], c = distHistory[2];
  if (a > b) { long t = a; a = b; b = t; }
  if (b > c) { long t = b; b = c; c = t; }
  if (a > b) { long t = a; a = b; b = t; }
  return b; // медиана
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
  lcd.setCursor(2, 0);
  lcd.write(slotL);
  lcd.write(slotR);
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

// === ПЛАВНАЯ BOOT АНИМАЦИЯ (state machine) ===
void startBootAnimation() {
  lcd.clear();
  lcd.backlight();
  wakePhase = WAKE_HALF;
  wakePhaseTime = millis();
  // Начинаем с закрытых глаз
  drawEyes(2, 3);
}

void updateBootAnimation() {
  unsigned long now = millis();
  unsigned long elapsed = now - wakePhaseTime;

  switch (wakePhase) {
    case WAKE_HALF:
      if (elapsed >= 400) {
        // Полузакрытые глаза
        lcd.createChar(0, EYE_HALF_L);
        lcd.createChar(1, EYE_HALF_R);
        drawEyes(0, 1);
        wakePhase = WAKE_OPEN;
        wakePhaseTime = now;
      }
      break;
    case WAKE_OPEN:
      if (elapsed >= 300) {
        loadDefaultEyes();
        drawEyes(0, 1);
        wakePhase = WAKE_BLINK1;
        wakePhaseTime = now;
      }
      break;
    case WAKE_BLINK1:
      if (elapsed >= 400) {
        drawEyes(2, 3);
        wakePhase = WAKE_OPEN2;
        wakePhaseTime = now;
      }
      break;
    case WAKE_OPEN2:
      if (elapsed >= 120) {
        loadDefaultEyes();
        drawEyes(0, 1);
        wakePhase = WAKE_BLINK2;
        wakePhaseTime = now;
      }
      break;
    case WAKE_BLINK2:
      if (elapsed >= 300) {
        drawEyes(2, 3);
        wakePhase = WAKE_FINAL;
        wakePhaseTime = now;
      }
      break;
    case WAKE_FINAL:
      if (elapsed >= 120) {
        loadDefaultEyes();
        drawEyes(0, 1);
        drawMouth(4);
        // Бегущая строка "ZHOPA ONLINE"
        const char* name = "  ZHOPA ONLINE  ";
        for (int i = 0; i < 16; i++) {
          lcd.setCursor(i, 1);
          lcd.print(name[i]);
          delay(40); // короткая задержка для эффекта
        }
        delay(700);
        drawFace();
        currentState = STATE_IDLE;
        wakePhase = WAKE_NONE;
      }
      break;
    default:
      break;
  }
}

// === ПЛАВНОЕ ЗАСЫПАНИЕ (state machine) ===
void startSleepAnimation() {
  sleepPhase = SLEEP_MOUTH;
  sleepPhaseTime = millis();
  animating = false;
  listening = false;
  drawMouth(4); // улыбка
}

void updateSleepAnimation() {
  unsigned long now = millis();
  unsigned long elapsed = now - sleepPhaseTime;

  switch (sleepPhase) {
    case SLEEP_MOUTH:
      if (elapsed >= 300) {
        // Полузакрытые глаза
        lcd.createChar(0, EYE_HALF_L);
        lcd.createChar(1, EYE_HALF_R);
        drawEyes(0, 1);
        sleepPhase = SLEEP_HALF;
        sleepPhaseTime = now;
      }
      break;
    case SLEEP_HALF:
      if (elapsed >= 400) {
        drawEyes(2, 3);
        sleepPhase = SLEEP_SHUT;
        sleepPhaseTime = now;
      }
      break;
    case SLEEP_SHUT:
      if (elapsed >= 500) {
        lcd.clear();
        drawEyes(2, 3);
        lcd.setCursor(6, 1);
        lcd.print("z Z z");
        sleepPhase = SLEEP_DONE;
        sleepPhaseTime = now;
      }
      break;
    case SLEEP_DONE:
      if (elapsed >= 300) {
        lcd.noBacklight();
        currentState = STATE_SLEEP;
        sleepPhase = SLEEP_NONE;
      }
      break;
    default:
      break;
  }
}

// === ЭКРАНЫ ===

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

// === ЗАГРУЗКА ЭМОЦИОНАЛЬНЫХ ГЛАЗ ===
void loadEmotionEyes(const char* emotion) {
  if (emotion[0] == '1') { // Удивление
    lcd.createChar(0, EYE_SURPRISE_L);
    lcd.createChar(1, EYE_SURPRISE_R);
    drawEyes(0, 1);
    drawMouth(6); // большой "О"
  } else if (emotion[0] == '2') { // Злость/дерзость
    lcd.createChar(0, EYE_ANGRY_L);
    lcd.createChar(1, EYE_ANGRY_R);
    drawEyes(0, 1);
  } else if (emotion[0] == '3') { // Смех (^_^)
    drawEyes(2, 3); // закрытые глаза = счастливые
    drawMouth(6); // рот открыт
  } else if (emotion[0] == '4') { // Подмигивание
    // Левый глаз открыт, правый закрыт
    lcd.setCursor(2, 0);
    lcd.write(byte(0));
    lcd.write(byte(1));
    lcd.setCursor(12, 0);
    lcd.write(byte(2));
    lcd.write(byte(3));
  }
  emotionActive = true;
  emotionStartTime = millis();
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
    // Плавное засыпание (без blocking delay!)
    startSleepAnimation();
  } else if (cmd == "S0") {
    lcd.backlight();
    startBootAnimation();
  } else if (cmd.startsWith("E")) {
    // Эмоции: E1=удивление, E2=злость, E3=смех, E4=подмигивание
    if (cmd.length() >= 2) {
      loadDefaultEyes(); // сначала загрузим default, потом заменим
      loadEmotionEyes(cmd.c_str() + 1);
    }
  } else if (cmd == "BL") {
    // Быстрое моргание — подтверждение
    drawEyes(2, 3);
    delay(80);
    loadDefaultEyes();
    drawEyes(0, 1);
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

  // Сразу включаемся с boot-анимацией
  startBootAnimation();
  wakeSent = true;
  sleepSent = false;
  lastFarTime = millis();
}

// === LOOP ===
void loop() {
  unsigned long now = millis();

  // --- Boot анимация (non-blocking, кроме бегущей строки) ---
  if (wakePhase != WAKE_NONE) {
    updateBootAnimation();
  }

  // --- Анимация засыпания (non-blocking) ---
  if (sleepPhase != SLEEP_NONE) {
    updateSleepAnimation();
  }

  // --- Serial ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      processCommand(inputBuffer);
      inputBuffer = "";
    } else if (c != '\r') {
      if (inputBuffer.length() < MAX_INPUT_LEN) {
        inputBuffer += c;
      } else {
        // Буфер переполнен — сбросить
        inputBuffer = "";
      }
    }
  }

  // --- HC-SR04 с фильтрацией ---
  if (now - lastMeasureTime >= MEASURE_MS) {
    lastMeasureTime = now;
    long rawDist = measureDistance();
    distHistory[distIdx] = rawDist;
    distIdx = (distIdx + 1) % 3;
    long dist = filteredDistance();

    if (dist < WAKE_DIST) {
      lastFarTime = now;
      if (!wakeSent) {
        Serial.println("WAKE");
        wakeSent = true;
        sleepSent = false;
        if (currentState == STATE_SLEEP) {
          lcd.backlight();
          startBootAnimation();
        }
      }
    } else {
      if (wakeSent && !sleepSent && (now - lastFarTime >= SLEEP_TIMEOUT)) {
        Serial.println("SLEEP");
        sleepSent = true;
        wakeSent = false;
        animating = false;
        listening = false;
        startSleepAnimation();
      }
    }
  }

  // --- Возврат из эмоции ---
  if (emotionActive && (now - emotionStartTime >= EMOTION_DUR)) {
    emotionActive = false;
    loadDefaultEyes();
    drawEyes(0, 1);
    drawMouth(4);
  }

  // --- МОРГАНИЕ ---
  if ((currentState == STATE_IDLE || currentState == STATE_TALKING) && !blinking && !looking && !emotionActive) {
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
  if (currentState == STATE_IDLE && !blinking && !looking && !emotionActive) {
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
