# Схема подключения Джарвис

## Компоненты
- Arduino Uno
- MAX7219 LED матрица 8x8
- HC-SR04 ультразвуковой датчик расстояния
- USB кабель (Arduino → ПК)
- Микрофон (подключён к ПК)
- Динамик / наушники (подключены к ПК)

## Подключение MAX7219 → Arduino Uno

```
MAX7219    Arduino
───────    ───────
VCC    →   5V
GND    →   GND
DIN    →   D12 (MOSI)
CS     →   D10 (SS)
CLK    →   D11 (SCK)
```

## Подключение HC-SR04 → Arduino Uno

```
HC-SR04    Arduino
───────    ───────
VCC    →   5V
GND    →   GND
TRIG   →   D7
ECHO   →   D6
```

## Визуальная схема

```
                    Arduino Uno
                 ┌───────────────┐
                 │               │
    MAX7219      │  D12 ← DIN   │     HC-SR04
   ┌───────┐     │  D11 ← CLK   │    ┌───────┐
   │ LED   │     │  D10 ← CS    │    │  )))   │
   │ 8x8   │     │               │    │       │
   │       │     │  D7  → TRIG   │    │  )))   │
   └───┬───┘     │  D6  ← ECHO  │    └───┬───┘
       │         │               │        │
       │  5V ────┤  5V           ├── 5V   │
       │  GND ───┤  GND          ├── GND  │
       │         │               │        │
                 │     USB       │
                 └───────┬───────┘
                         │
                    ┌────┴────┐
                    │   ПК    │
                    │ Python  │
                    │ 🎤 🔊   │
                    └─────────┘
```

## Питание
- Arduino питается от USB (5V от ПК)
- MAX7219 и HC-SR04 питаются от 5V Arduino
- При ярком свечении всей матрицы может потребоваться внешнее питание 5V

## Библиотека Arduino
- Установить через Arduino IDE: Sketch → Include Library → Manage Libraries → "LedControl"
