"""
Джарвис — ИИ-ассистент.

Режимы:
  python main.py              — текстовый режим (сервер, без микрофона)
  python main.py --voice      — голосовой режим (ПК с микрофоном и динамиком)
  python main.py --voice COM9 — голосовой + Arduino на порту

Переменная окружения: OPENAI_API_KEY
"""

import sys
import time

from ai import JarvisAI


def run_text_mode():
    """Текстовый режим — для тестирования на сервере."""
    print("=" * 50)
    print("  ДЖАРВИС — ИИ-ассистент (текстовый режим)")
    print("=" * 50)
    print("  Пиши сообщения. 'выход' — завершить.\n")

    ai = JarvisAI()

    while True:
        try:
            text = input("Ты: ").strip()
        except (EOFError, KeyboardInterrupt):
            break

        if not text:
            continue

        if text.lower() in ("выход", "exit", "quit", "стоп"):
            print("\n[Джарвис] До свидания!")
            break

        answer = ai.ask(text)
        print(f"\nДжарвис: {answer}\n")


def run_voice_mode(port=None):
    """Голосовой режим — ПК с микрофоном/динамиком и опционально Arduino."""
    from serial_comm import ArduinoSerial
    from speech import SpeechRecognizer
    from tts import TextToSpeech

    print("=" * 50)
    print("  ДЖАРВИС — ИИ-ассистент (голосовой режим)")
    print("=" * 50)

    # --- Инициализация ---
    arduino = ArduinoSerial(port=port)
    if not arduino.connected:
        print("\n[!] Arduino не подключена — работаю без дисплея.")
        print("    Укажи порт: python main.py --voice COM9\n")

    recognizer = SpeechRecognizer(language="ru-RU")
    ai = JarvisAI()
    tts = TextToSpeech()

    # TTS callback для анимации рта
    if arduino.connected:
        tts.on_start(lambda: arduino.start_talking_animation())
        tts.on_end(lambda: arduino.stop_animation())

    # --- Калибровка микрофона ---
    recognizer.calibrate(duration=1)

    # --- Приветствие ---
    print("\n[Джарвис] Система готова. Говори!\n")
    if arduino.connected:
        time.sleep(0.5)  # дождаться boot-анимации Arduino
        arduino.start_talking_animation()
    tts.speak("Джарвис онлайн. Чем могу помочь?")
    if arduino.connected:
        arduino.stop_animation()

    # --- Главный цикл ---
    try:
        while True:
            # Слушаю
            if arduino.connected:
                arduino.start_listening_animation()

            text = recognizer.listen(timeout=10, phrase_time_limit=15)

            if text is None:
                if arduino.connected:
                    arduino.mouth_closed()
                continue

            print(f"[Ты]: {text}")

            # Команда выхода
            lower = text.lower()
            if any(cmd in lower for cmd in ["выключись", "пока", "до свидания", "стоп"]):
                if arduino.connected:
                    arduino.start_talking_animation()
                tts.speak("До свидания!")
                if arduino.connected:
                    arduino.stop_animation()
                    arduino.sleep_mode()
                break

            # Получить ответ AI
            if arduino.connected:
                arduino.mouth_closed()

            answer = ai.ask(text)

            # Озвучить (анимация через TTS callback)
            if arduino.connected:
                arduino.start_talking_animation()
            tts.speak(answer)
            if arduino.connected:
                arduino.stop_animation()
                arduino.mouth_closed()

    except KeyboardInterrupt:
        print("\n\n[Джарвис] Выключаюсь...")
    finally:
        if arduino.connected:
            arduino.stop_animation()
            arduino.sleep_mode()
            time.sleep(0.3)
            arduino.close()
        print("[Джарвис] До свидания!")


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
