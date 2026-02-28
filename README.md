# 🤖 Jarvis — Arduino AI Assistant

Голосовой ИИ-ассистент с анимацией рта на LED-матрице 8x8.

- **Arduino Uno** управляет матрицей MAX7219 и датчиком расстояния HC-SR04
- **Python на ПК** слушает микрофон, общается с OpenAI GPT, озвучивает ответ
- Матрица **оживает** когда ты подходишь, и **засыпает** когда уходишь

---

## Демо

```
[Человек подходит] → HC-SR04 будит систему
       ↓
[Матрица]: анимация "слушаю"
       ↓
[Микрофон]: распознавание речи (Google, русский)
       ↓
[OpenAI GPT]: генерация ответа
       ↓
[Матрица]: анимация рта  +  [Динамик]: озвучка
       ↓
[Человек уходит] → система засыпает
```

---

## Железо

| Компонент | Описание |
|-----------|----------|
| Arduino Uno | Основная плата |
| MAX7219 8x8 LED матрица | Анимация рта |
| HC-SR04 | Датчик расстояния — автопробуждение |
| Микрофон | Подключён к ПК |
| Динамик / наушники | Подключены к ПК |
| USB кабель | Arduino ↔ ПК |

---

## Схема подключения

### MAX7219 → Arduino Uno

```
MAX7219    Arduino Uno
───────    ───────────
VCC    →   5V
GND    →   GND
DIN    →   D12
CS     →   D10
CLK    →   D11
```

### HC-SR04 → Arduino Uno

```
HC-SR04    Arduino Uno
───────    ───────────
VCC    →   5V
GND    →   GND
TRIG   →   D7
ECHO   →   D6
```

### Визуальная схема

```
                  Arduino Uno
               ┌─────────────────┐
               │   D12 ← DIN     │◄── MAX7219
               │   D11 ← CLK     │◄── MAX7219
               │   D10 ← CS      │◄── MAX7219
               │                 │
               │   D7  → TRIG    │──► HC-SR04
               │   D6  ← ECHO    │◄── HC-SR04
               │                 │
               │   5V  ──────────┼──► MAX7219 VCC
               │   5V  ──────────┼──► HC-SR04 VCC
               │   GND ──────────┼──► MAX7219 GND
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

---

## Установка

### 1. Arduino

1. Открой Arduino IDE
2. Установи библиотеку **LedControl**:
   `Sketch → Include Library → Manage Libraries → "LedControl"`
3. Открой `arduino/jarvis_mouth.ino`
4. Выбери `Tools → Board → Arduino Uno`
5. Выбери нужный порт (`Tools → Port`)
6. Нажми **Upload**

### 2. Python (ПК)

**Требования:** Python 3.8+

```bash
# Клонировать репозиторий
git clone https://github.com/Tanjiro00/Jarvis_arduino.git
cd Jarvis_arduino/python

# Установить зависимости для голосового режима
# На Linux сначала:
sudo apt install portaudio19-dev

pip install openai SpeechRecognition pyaudio pyttsx3 pyserial
```

**Получить API-ключ OpenAI:** https://platform.openai.com/api-keys

---

## Запуск

### Текстовый режим (тест без микрофона)

```bash
cd python
export OPENAI_API_KEY="sk-proj-..."   # Linux / Mac
# или
set OPENAI_API_KEY=sk-proj-...        # Windows CMD

python main.py
```

Пишешь текст — Джарвис отвечает. Введи `выход` для завершения.

### Голосовой режим (ПК с микрофоном + Arduino)

```bash
# Arduino определится автоматически
python main.py --voice

# Или указать порт вручную
python main.py --voice COM3        # Windows
python main.py --voice /dev/ttyUSB0  # Linux
```

---

## Анимации матрицы

| Команда | Анимация |
|---------|----------|
| `M0` | Рот закрыт (idle) |
| `M1`–`M4` | Рот открывается от малого до максимума |
| `L1` | Пульсация "слушаю" |
| `A1` | Автоанимация разговора (цикл кадров) |
| `A0` | Стоп анимация |
| `S1` | Режим сна |
| `S0` | Выход из сна |

Arduino → ПК:
- `WAKE` — человек подошёл (HC-SR04 < 100 см)
- `SLEEP` — человек ушёл (нет никого 10 сек)

---

## Структура проекта

```
Jarvis_arduino/
├── arduino/
│   └── jarvis_mouth.ino    # Скетч Arduino
├── python/
│   ├── main.py             # Главный скрипт
│   ├── ai.py               # OpenAI GPT
│   ├── speech.py           # Распознавание речи
│   ├── tts.py              # Синтез речи
│   ├── serial_comm.py      # Serial-связь с Arduino
│   └── requirements.txt    # Зависимости
└── schemas/
    └── wiring.md           # Подробная схема подключения
```

---

## Настройки

### Изменить расстояние активации

В `arduino/jarvis_mouth.ino`, строка:
```cpp
const int WAKE_DISTANCE_CM = 100;  // порог в сантиметрах
```

### Изменить задержку перед сном

```cpp
const unsigned long SLEEP_DELAY = 10000;  // 10 секунд в мс
```

### Изменить модель GPT

В `python/ai.py`:
```python
model="gpt-4o-mini"   # быстро и дёшево
# или
model="gpt-4o"        # умнее, дороже
```

### Изменить характер Джарвиса

В `python/ai.py`, переменная `SYSTEM_PROMPT`:
```python
SYSTEM_PROMPT = "Ты — Джарвис, умный и лаконичный ИИ-ассистент. ..."
```

---

## Частые проблемы

**`portaudio.h: No such file or directory`** при установке pyaudio:
```bash
sudo apt install portaudio19-dev   # Ubuntu/Debian
brew install portaudio             # Mac
```

**Arduino не найдена автоматически:**
```bash
python main.py --voice /dev/ttyUSB0   # Linux
python main.py --voice COM3           # Windows
```

**Нет русского голоса в pyttsx3 на Linux:**
```bash
sudo apt install espeak espeak-data
```

**Плохо слышит микрофон:**
В `python/speech.py` уменьши порог чувствительности:
```python
self.recognizer.energy_threshold = 200  # по умолчанию 300
```

---

## Требования

- Python 3.8+
- Arduino IDE 1.8+ или 2.x
- OpenAI API ключ
- Интернет (для Google Speech Recognition и OpenAI API)
