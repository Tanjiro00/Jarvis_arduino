#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// === HC-SR04 ===
const int TRIG_PIN = 7;
const int ECHO_PIN = 6;
const int WAKE_DIST = 80;            // см
const unsigned long SLEEP_TIMEOUT = 12000;
const unsigned long MEASURE_MS    = 250;

// === ТАЙМИНГИ АНИМАЦИИ ===
const unsigned long TALK_MS   = 100;
const unsigned long LISTEN_MS = 500;
const unsigned long BLINK_INTERVAL_MIN = 3000;
const unsigned long BLINK_INTERVAL_MAX = 7000;
const unsigned long BLINK_DURATION     = 150;

// =====================================================
// КАСТОМНЫЕ СИМВОЛЫ — ЛИЦО
// На 16x2 LCD: строка 0 = глаза, строка 1 = рот
// Глаза: 2 символа (позиции 4-5 и 10-11)
// Рот: 2 символа (позиции 7-8)
// =====================================================

// --- ГЛАЗА ---
// Слот 0: глаз открыт (левый и правый одинаковые)
byte EYE_OPEN[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b10101,
  0b10101,
  0b10001,
  0b01110,
  0b00000
};

// Слот 1: глаз закрыт (моргание)
byte EYE_SHUT[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b11111,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

// Слот 2: глаз — смотрит влево
byte EYE_LEFT[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b10100,
  0b10100,
  0b10001,
  0b01110,
  0b00000
};

// Слот 3: глаз — смотрит вправо
byte EYE_RIGHT[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b00101,
  0b00101,
  0b10001,
  0b01110,
  0b00000
};

// --- РОТ ---
// Слот 4: рот закрыт — улыбка
byte MOUTH_SMILE[8] = {
  0b00000,
  0b00000,
  0b10001,
  0b01110,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

// Слот 5: рот приоткрыт — маленький "о"
byte MOUTH_SM[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b10001,
  0b01110,
  0b00000,
  0b00000,
  0b00000
};

// Слот 6: рот широко — большой "О"
byte MOUTH_BIG[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b10001,
  0b10001,
  0b01110,
  0b00000,
  0b00000
};

// Слот 7: рот — слушает (точки)
byte MOUTH_LISTEN[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b10101,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

// === АНИМАЦИЯ РАЗГОВОРА ===
// Слоты рта: 4=smile, 5=small, 6=big
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
unsigned long lastFarTime     = 0;
bool wakeSent  = false;
bool sleepSent = true;

// Моргание
unsigned long nextBlinkTime = 0;
bool blinking = false;
unsigned long blinkStartTime = 0;

// Listening dot animation
int listenDotCount = 0;

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
  nextBlinkTime = millis() + random(BLINK_INTERVAL_MIN, BLINK_INTERVAL_MAX);
}

void drawEyes(byte charSlot) {
  // Глаза на строке 0: позиции 3-4 и 11-12
  lcd.setCursor(3, 0);
  lcd.write(charSlot);
  lcd.write(charSlot);
  lcd.setCursor(11, 0);
  lcd.write(charSlot);
  lcd.write(charSlot);
}

void drawMouth(byte charSlot) {
  // Рот на строке 1: позиции 7-8
  lcd.setCursor(7, 1);
  lcd.write(charSlot);
  lcd.write(charSlot);
}

void drawFace() {
  // Полное лицо: глаза + рот
  lcd.clear();
  drawEyes(0);   // открытые глаза
  drawMouth(4);   // улыбка
  scheduleNextBlink();
}

// === BOOT ===
void bootAnimation() {
  lcd.clear();
  lcd.backlight();

  // Фаза 1: глаза "просыпаются" — от закрытых к открытым
  drawEyes(1);  // закрытые
  delay(400);
  drawEyes(0);  // открытые
  delay(200);
  drawEyes(1);  // моргнули
  delay(150);
  drawEyes(0);  // открыли
  delay(300);

  // Фаза 2: улыбка появляется
  drawMouth(4);
  delay(500);

  // Фаза 3: бегущая строка имени внизу
  const char* name = " ZHOPA ONLINE ";
  for (int i = 0; i < 14; i++) {
    lcd.setCursor(1 + i, 1);
    lcd.print(name[i]);
    delay(50);
  }
  delay(800);

  // Убираем текст, оставляем лицо
  drawFace();
}

// === ЭКРАНЫ ===

void showSleep() {
  lcd.clear();
  // Спящее лицо: закрытые глаза + Z
  drawEyes(1);
  lcd.setCursor(7, 1);
  lcd.print("zZ");
  lcd.noBacklight();
}

void showIdle() {
  lcd.backlight();
  drawFace();
  currentState = STATE_IDLE;
}

void showListening() {
  // Глаза широко + рот "слушает"
  drawEyes(0);
  lcd.setCursor(0, 1);
  lcd.print("                ");
  drawMouth(7);  // точки
  listenDotCount = 0;
  currentState = STATE_LISTENING;
}

void showSpeaking() {
  drawEyes(0);
  currentState = STATE_TALKING;
}

// === ОБРАБОТКА КОМАНД ===
void processCommand(String cmd) {
  cmd.trim();

  if (cmd == "M0") {
    animating = false; listening = false;
    drawMouth(4);  // улыбка
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
    // Засыпание: глаза закрываются
    drawMouth(4);
    delay(200);
    drawEyes(1);  // закрыли глаза
    delay(400);
    showSleep();
    currentState = STATE_SLEEP;
  } else if (cmd == "S0") {
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

  lcd.createChar(0, EYE_OPEN);
  lcd.createChar(1, EYE_SHUT);
  lcd.createChar(2, EYE_LEFT);
  lcd.createChar(3, EYE_RIGHT);
  lcd.createChar(4, MOUTH_SMILE);
  lcd.createChar(5, MOUTH_SM);
  lcd.createChar(6, MOUTH_BIG);
  lcd.createChar(7, MOUTH_LISTEN);

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
        wakeSent  = true;
        sleepSent = false;
        if (currentState == STATE_SLEEP) {
          bootAnimation();
          showIdle();
        }
      }
    } else {
      if (wakeSent && !sleepSent && (now - lastFarTime >= SLEEP_TIMEOUT)) {
        Serial.println("SLEEP");
        sleepSent = true;
        wakeSent  = false;
        animating = false;
        listening = false;
        // Засыпание
        drawEyes(1);
        delay(300);
        showSleep();
        currentState = STATE_SLEEP;
      }
    }
  }

  // --- МОРГАНИЕ (idle и talking) ---
  if ((currentState == STATE_IDLE || currentState == STATE_TALKING) && !blinking) {
    if (now >= nextBlinkTime) {
      blinking = true;
      blinkStartTime = now;
      drawEyes(1);  // закрыть
    }
  }
  if (blinking && (now - blinkStartTime >= BLINK_DURATION)) {
    blinking = false;
    drawEyes(0);  // открыть
    scheduleNextBlink();
  }

  // --- IDLE: случайный взгляд ---
  if (currentState == STATE_IDLE && !blinking) {
    // Иногда смотрит влево/вправо
    static unsigned long lastLookTime = 0;
    static bool looking = false;
    if (!looking && (now - lastLookTime > random(5000, 10000))) {
      looking = true;
      lastLookTime = now;
      byte dir = random(0, 3); // 0=лево, 1=право, 2=прямо
      if (dir == 0) drawEyes(2);
      else if (dir == 1) drawEyes(3);
    }
    if (looking && (now - lastLookTime > 800)) {
      looking = false;
      drawEyes(0);  // назад прямо
      lastLookTime = now;
    }
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
    // Точки появляются и исчезают
    listenDotCount = (listenDotCount + 1) % 4;
    lcd.print("      ");  // очистить
    lcd.setCursor(5, 1);
    for (int i = 0; i < listenDotCount + 1; i++) {
      lcd.write(byte(7));
      lcd.print(" ");
    }
  }
}
