"""
ЖОПА — Жутко Оптимизированный Персональный Ассистент.

Работает так:
1. HC-SR04 обнаруживает человека → дисплей просыпается
2. Слушает микрофон, ждёт имя "ЖОПА" в начале фразы
3. Распознаёт команду → GPT → озвучивает ответ
4. Человек уходит → засыпает

Запуск:
  python main.py              — текстовый режим
  python main.py --voice COM9 — голосовой + Arduino
"""

import sys
import time
import threading

from ai import JarvisAI

# Ключевые слова для активации (как произносит Whisper)
WAKE_WORDS = ["жопа", "жопу", "жоп", "zhopa"]


def contains_wake_word(text):
    """Проверяет есть ли имя ЖОПА в тексте."""
    lower = text.lower()
    for word in WAKE_WORDS:
        if word in lower:
            return True
    return False


def strip_wake_word(text):
    """Убирает имя из начала фразы, оставляет команду."""
    lower = text.lower()
    for word in WAKE_WORDS:
        pos = lower.find(word)
        if pos != -1:
            after = text[pos + len(word):].strip(" ,!.?")
            if after:
                return after
    return text


def run_text_mode():
    """Текстовый режим."""
    print("=" * 50)
    print("  ЖОПА — текстовый режим")
    print("  Начинай фразу с 'ЖОПА' для активации")
    print("=" * 50)
    print("  Примеры: 'ЖОПА, как дела?', 'ЖОПА, кто ты?'")
    print("  'выход' — завершить.\n")

    ai = JarvisAI()

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


def run_voice_mode(port=None):
    """Голосовой режим с Arduino."""
    from serial_comm import ArduinoSerial
    from speech import SpeechRecognizer
    from tts import TextToSpeech

    print("=" * 50)
    print("  ЖОПА — голосовой режим")
    print("  Скажи 'ЖОПА' + команду для активации")
    print("=" * 50)

    # --- Инициализация ---
    arduino = ArduinoSerial(port=port)
    if not arduino.connected:
        print("\n[!] Arduino не подключена — без дисплея и датчика.")
        print("    Укажи порт: python main.py --voice COM9\n")

    recognizer = SpeechRecognizer(language="ru")
    ai = JarvisAI()
    tts = TextToSpeech(voice="onyx")

    # Анимация рта
    if arduino.connected:
        tts.on_start(lambda: arduino.start_talking_animation())
        tts.on_end(lambda: arduino.stop_animation())

    # --- Состояния ---
    is_awake = threading.Event()

    if arduino.connected:
        def on_wake():
            if not is_awake.is_set():
                print("\n[Датчик] Кто-то подошёл!")
                is_awake.set()

        def on_sleep():
            print("\n[Датчик] Ушёл. Засыпаю...")
            is_awake.clear()
            ai.clear_history()

        arduino.on_wake(on_wake)
        arduino.on_sleep(on_sleep)
        arduino.start_reading()
    else:
        is_awake.set()  # без Arduino — всегда активен

    # Калибровка
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

            text = recognizer.listen(timeout=8, phrase_time_limit=15)

            if text is None:
                if arduino.connected:
                    arduino.mouth_closed()
                continue

            print(f"\n[Mic] {text}")

            # Проверяем имя
            if not contains_wake_word(text):
                print("[...] Нет имени — игнорирую")
                if arduino.connected:
                    arduino.mouth_closed()
                continue

            # Извлекаем команду
            command = strip_wake_word(text)

            # Команда выхода
            lower = command.lower()
            if any(cmd in lower for cmd in ["выключись", "спи", "до свидания"]):
                tts.speak("Ладно, братан. Отдыхай.")
                if arduino.connected:
                    arduino.sleep_mode()
                    is_awake.clear()
                continue

            # Если сказали просто "ЖОПА" без команды
            if not command or len(command) < 2:
                command = "тебя позвали, ответь коротко что ты тут"

            # AI ответ
            if arduino.connected:
                arduino.mouth_closed()

            answer = ai.ask(command)

            # Озвучка
            tts.speak(answer)

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
