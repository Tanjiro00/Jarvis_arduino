"""
ЖОПА — Жутко Оптимизированный Персональный Ассистент.

Работает так:
1. HC-SR04 обнаруживает человека → дисплей просыпается + приветствие
2. Слушает микрофон, ждёт имя "ЖОПА" в начале фразы
3. Beep → распознаёт команду → GPT streaming → TTS по предложениям
4. Человек уходит → засыпает + сохраняет память

Запуск:
  python main.py              — текстовый режим
  python main.py --voice COM9 — голосовой + Arduino
"""

import random
import re
import sys
import time
import threading

from openai import OpenAI

import config
from ai import JarvisAI

# Regex для wake-word (как отдельное слово)
WAKE_PATTERN = re.compile(r'\bжоп[ауеы]?\b|\bzhopa\b', re.IGNORECASE)


def contains_wake_word(text):
    """Проверяет есть ли имя ЖОПА в тексте (regex, не substring)."""
    return bool(WAKE_PATTERN.search(text))


def strip_wake_word(text):
    """Убирает имя из фразы, оставляет команду."""
    # Находим wake-word и берём всё после него
    match = WAKE_PATTERN.search(text)
    if match:
        after = text[match.end():].strip(" ,!.?")
        if after:
            return after
    return text


# === ТЕКСТОВЫЙ РЕЖИМ ===

def run_text_mode():
    """Текстовый режим для тестирования без микрофона."""
    print("=" * 50)
    print("  ЖОПА — текстовый режим")
    print("  Начинай фразу с 'ЖОПА' для активации")
    print("=" * 50)
    print("  Примеры: 'ЖОПА, как дела?', 'ЖОПА, кто ты?'")
    print("  'выход' — завершить.\n")

    client = OpenAI(api_key=config.OPENAI_API_KEY)
    ai = JarvisAI(client=client)

    while True:
        try:
            text = input("Ты: ").strip()
        except (EOFError, KeyboardInterrupt):
            break

        if not text:
            continue

        if text.lower() in ("выход", "exit", "quit"):
            print("\n[ЖОПА] Пока, братан!")
            break

        if not contains_wake_word(text):
            print("[...] Скажи 'ЖОПА' чтобы я ответил.\n")
            continue

        command = strip_wake_word(text)
        if not command or command.lower() == text.lower():
            command = "привет"

        answer = ai.ask(command)
        print(f"\nЖОПА: {answer}\n")


# === ГОЛОСОВОЙ РЕЖИМ ===

def run_voice_mode(port=None):
    """Голосовой режим с Arduino и streaming."""
    from serial_comm import ArduinoSerial
    from speech import SpeechRecognizer
    from tts import TextToSpeech

    print("=" * 50)
    print("  ЖОПА — голосовой режим")
    print("  Скажи 'ЖОПА' + команду для активации")
    print("=" * 50)

    # --- Единый OpenAI клиент ---
    client = OpenAI(api_key=config.OPENAI_API_KEY)

    # --- Инициализация ---
    arduino = ArduinoSerial(port=port)
    if not arduino.connected:
        print("\n[!] Arduino не подключена — без дисплея и датчика.")
        print("    Укажи порт: python main.py --voice COM9\n")

    recognizer = SpeechRecognizer(client=client, language="ru")
    ai = JarvisAI(client=client)
    tts = TextToSpeech(client=client)

    # Прогрев HTTP соединения
    ai.warmup()

    # --- Состояния ---
    is_awake = threading.Event()

    if arduino.connected:
        def on_wake():
            if not is_awake.is_set():
                print("\n[Датчик] Кто-то подошёл!")
                is_awake.set()
                # Звук пробуждения + приветствие в отдельном потоке
                threading.Thread(target=_wake_greeting, args=(tts, arduino), daemon=True).start()

        def on_sleep():
            print("\n[Датчик] Ушёл. Засыпаю...")
            is_awake.clear()
            ai.clear_history()  # сохраняет память и чистит историю
            tts.play_sleep_sound()

        arduino.on_wake(on_wake)
        arduino.on_sleep(on_sleep)
        arduino.start_reading()

        # Анимация рта привязана к TTS
        tts.on_start(lambda: arduino.start_talking_animation())
        tts.on_end(lambda: arduino.stop_animation())
    else:
        is_awake.set()  # без Arduino — всегда активен

    # Калибровка микрофона
    recognizer.calibrate(duration=1)

    print("\n[ЖОПА] Система готова.")
    if arduino.connected:
        print("[ЖОПА] Жду пока кто-нибудь подойдёт...\n")
    else:
        print("[ЖОПА] Скажи 'ЖОПА' + вопрос.\n")

    # --- Главный цикл ---
    try:
        while True:
            # Ждём пробуждения от датчика
            if not is_awake.is_set():
                is_awake.wait(timeout=1)
                continue

            # Слушаем
            if arduino.connected:
                arduino.start_listening_animation()

            text = recognizer.listen()

            if text is None:
                if arduino.connected:
                    arduino.mouth_closed()
                continue

            print(f"\n[Mic] {text}")

            # Проверяем wake-word
            if not contains_wake_word(text):
                print("[...] Нет имени — игнорирую")
                if arduino.connected:
                    arduino.mouth_closed()
                continue

            # Beep — подтверждение что услышал
            tts.play_beep()
            if arduino.connected:
                arduino.blink_confirm()

            # Извлекаем команду
            command = strip_wake_word(text)

            # Команда выхода
            lower = command.lower()
            if any(cmd in lower for cmd in ["выключись", "спи", "до свидания"]):
                tts.speak("Ладно, братан. Отдыхай.")
                if arduino.connected:
                    arduino.sleep_mode()
                    is_awake.clear()
                    ai.clear_history()
                continue

            # Если сказали просто "ЖОПА" без команды
            if not command or len(command) < 2:
                command = "тебя позвали, ответь коротко что ты тут"

            # AI ответ — STREAMING
            if arduino.connected:
                arduino.mouth_closed()

            full_answer = ""
            is_first = True
            last_sentence = None

            for sentence in ai.ask_stream(command):
                if last_sentence is not None:
                    tts.speak_chunk(last_sentence, is_first=is_first, is_last=False)
                    is_first = False
                last_sentence = sentence
                full_answer += sentence + " "

            # Последнее предложение
            if last_sentence is not None:
                tts.speak_chunk(last_sentence, is_first=is_first, is_last=True)

            # Эмоция на дисплее
            if arduino.connected and full_answer:
                emotion = ai.detect_emotion(full_answer)
                if emotion:
                    arduino.set_emotion(emotion)

            if arduino.connected:
                arduino.mouth_closed()

    except KeyboardInterrupt:
        print("\n\n[ЖОПА] Выключаюсь...")
    finally:
        if arduino.connected:
            arduino.stop_animation()
            arduino.sleep_mode()
            time.sleep(0.3)
            arduino.close()
        recognizer.close()
        print("[ЖОПА] Пока, братан!")


def _wake_greeting(tts, arduino):
    """Приветствие при пробуждении (запускается в отдельном потоке)."""
    try:
        tts.play_wake_sound()
        greeting = random.choice(config.WAKE_GREETINGS)
        tts.speak(greeting)
    except Exception as e:
        print(f"[Wake] Ошибка приветствия: {e}")


# === ENTRY POINT ===

def main():
    args = sys.argv[1:]

    if "--voice" in args:
        args.remove("--voice")
        port = args[0] if args else None
        run_voice_mode(port=port)
    else:
        run_text_mode()


if __name__ == "__main__":
    main()
