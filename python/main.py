"""
Джарвис — ИИ-ассистент.

Режимы:
  python main.py              — текстовый режим (сервер, без микрофона)
  python main.py --voice      — голосовой режим (ПК с микрофоном и динамиком)
  python main.py --voice COM3 — голосовой + Arduino на порту COM3

Переменная окружения: OPENAI_API_KEY
"""

import sys
import threading

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
    """Голосовой режим — для ПК с микрофоном/динамиком и опционально Arduino."""
    from serial_comm import ArduinoSerial
    from speech import SpeechRecognizer
    from tts import TextToSpeech

    print("=" * 50)
    print("  ДЖАРВИС — ИИ-ассистент (голосовой режим)")
    print("=" * 50)

    # --- Инициализация ---
    arduino = ArduinoSerial(port=port)
    if not arduino.connected:
        print("\n[!] Arduino не подключена — работаю без матрицы и датчика.")
        print("    Для подключения: python main.py --voice /dev/ttyUSB0\n")

    recognizer = SpeechRecognizer(language="ru-RU")
    ai = JarvisAI()
    tts = TextToSpeech()

    # --- Callback'и ---
    is_awake = threading.Event()

    if arduino.connected:
        def on_wake():
            if not is_awake.is_set():
                print("\n[Датчик] Человек обнаружен!")
                is_awake.set()

        def on_sleep():
            print("\n[Датчик] Человек ушёл. Засыпаю...")
            is_awake.clear()
            ai.clear_history()

        arduino.on_wake(on_wake)
        arduino.on_sleep(on_sleep)
        arduino.start_reading()

        tts.on_start(lambda: arduino.start_talking_animation())
        tts.on_end(lambda: arduino.stop_animation())
    else:
        is_awake.set()

    # --- Калибровка ---
    recognizer.calibrate(duration=1)

    print("\n[Джарвис] Система готова.")
    if arduino.connected:
        print("[Джарвис] Жду, когда кто-нибудь подойдёт...\n")
        arduino.sleep_mode()
    else:
        print("[Джарвис] Слушаю...\n")

    # --- Главный цикл ---
    try:
        while True:
            if not is_awake.is_set():
                is_awake.wait(timeout=1)
                continue

            if arduino.connected:
                arduino.wake_mode()
                arduino.start_talking_animation()
                tts.speak("Здравствуйте. Чем могу помочь?")
                arduino.stop_animation()

            while is_awake.is_set():
                if arduino.connected:
                    arduino.start_listening_animation()

                text = recognizer.listen(timeout=10, phrase_time_limit=15)

                if text is None:
                    if arduino.connected:
                        arduino.mouth_closed()
                    continue

                lower = text.lower()
                if any(cmd in lower for cmd in ["выключись", "пока", "до свидания", "стоп"]):
                    if arduino.connected:
                        arduino.start_talking_animation()
                    tts.speak("До свидания!")
                    if arduino.connected:
                        arduino.stop_animation()
                        arduino.sleep_mode()
                    ai.clear_history()
                    is_awake.clear()
                    break

                if arduino.connected:
                    arduino.mouth_closed()

                answer = ai.ask(text)

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
