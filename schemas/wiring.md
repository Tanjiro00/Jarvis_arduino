# Схема подключения Джарвис

## Компоненты
- Arduino Uno
- LCD 16x2 с I2C-модулем (PCF8574, адрес 0x27 или 0x3F)
- HC-SR04 ультразвуковой датчик расстояния
- USB кабель (Arduino → ПК)
- Микрофон (подключён к ПК)
- Динамик / наушники (подключены к ПК)

## Подключение LCD I2C → Arduino Uno

```
LCD I2C    Arduino Uno
───────    ───────────
VCC    →   5V
GND    →   GND
SDA    →   A4
SCL    →   A5
```

## Подключение HC-SR04 → Arduino Uno

```
HC-SR04    Arduino Uno
───────    ───────────
VCC    →   5V
GND    →   GND
TRIG   →   D7
ECHO   →   D6
```

## Визуальная схема

```
                  Arduino Uno
               ┌─────────────────┐
               │   A4  ← SDA     │◄── LCD I2C
               │   A5  ← SCL     │◄── LCD I2C
               │                 │
               │   D7  → TRIG    │──► HC-SR04
               │   D6  ← ECHO    │◄── HC-SR04
               │                 │
               │   5V  ──────────┼──► LCD VCC
               │   5V  ──────────┼──► HC-SR04 VCC
               │   GND ──────────┼──► LCD GND
               │   GND ──────────┼──► HC-SR04 GND
               │                 │
               │      USB        │
               └────────┬────────┘
                        │
                   ┌────┴────┐
                   │   ПК    │
                   │ Python  │
                   │  🎤 🔊  │
                   └─────────┘
```

## Что отображается на дисплее

```
┌────────────────┐
│  [ JARVIS ]    │  ← строка 0: статус
│       ()       │  ← строка 1: анимация рта (cols 7-8)
└────────────────┘
```

Режимы строки 0:
- `    [ JARVIS ]  ` — ожидание
- `   [ LISTENING ]` — распознавание речи
- `   [ SPEAKING ] ` — Джарвис говорит

Режим сна:
```
┌────────────────┐
│     zzZ...     │
│      ~_~       │
└────────────────┘
```

## Определение I2C-адреса

Если дисплей не работает — загрузи I2C Scanner скетч:

```cpp
#include <Wire.h>
void setup() {
  Wire.begin();
  Serial.begin(9600);
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found: 0x");
      Serial.println(addr, HEX);
    }
  }
}
void loop() {}
```

Найденный адрес подставь в строку скетча:
```cpp
LiquidCrystal_I2C lcd(0x27, 16, 2); // ← сюда
```

## Библиотека Arduino
Установить через Arduino IDE:
`Sketch → Include Library → Manage Libraries → "LiquidCrystal I2C"`
(by Frank de Brabander или johnrickman)
