"""
ЖОПА — Жутко Оптимизированный Персональный Ассистент.

Режимы:
  python main.py              — текстовый (без микрофона)
  python main.py --voice      — голосовой (без Arduino)
  python main.py --voice COM9 — голосовой + Arduino LCD

Переменная окружения: OPENAI_API_KEY
"""

import sys
import time

from ai import JarvisAI


def run_text_mode():
    """Текстовый режим."""
    print("=" * 50)
    print("  ЖОПА — текстовый режим")
    print("  Жутко Оптимизированный Персональный Ассистент")
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
            print("\n[ЖОПА] Пока!")
            break

        answer = ai.ask(text)
        print(f"\nЖОПА: {answer}\n")


def run_voice_mode(port=None):
    """Голосовой режим."""
    from serial_comm import ArduinoSerial
    from speech import SpeechRecognizer
    from tts import TextToSpeech

    print("=" * 50)
    print("  ЖОПА — голосовой режим")
    print("  Жутко Оптимизированный Персональный Ассистент")
    print("=" * 50)

    # --- Инициализация ---
    arduino = ArduinoSerial(port=port)
    if not arduino.connected:
        print("\n[!] Arduino не подключена — работаю без дисплея.")
        print("    Укажи порт: python main.py --voice COM9\n")

    recognizer = SpeechRecognizer(language="ru")
    ai = JarvisAI()
    tts = TextToSpeech(voice="onyx")  # низкий мужской голос

    # Анимация рта привязана к TTS
    if arduino.connected:
        tts.on_start(lambda: arduino.start_talking_animation())
        tts.on_end(lambda: arduino.stop_animation())

    # Калибровка
    recognizer.calibrate(duration=1)

    # Приветствие
    print("\n[ЖОПА] Система запущена!\n")
    if arduino.connected:
        time.sleep(3)  # ждём boot-анимацию Arduino
    tts.speak("ЖОПА онлайн. Слушаю.")

    # --- Главный цикл ---
    try:
        while True:
            # Анимация слушания
            if arduino.connected:
                arduino.start_listening_animation()

            # Записать и распознать речь
            text = recognizer.listen(timeout=8, phrase_time_limit=15)

            if text is None:
                if arduino.connected:
                    arduino.mouth_closed()
                continue

            print(f"\n[Ты] {text}")

            # Выход
            lower = text.lower()
            if any(cmd in lower for cmd in ["выключись", "пока", "до свидания", "выход"]):
                tts.speak("Пока! Если что — я тут.")
                if arduino.connected:
                    arduino.sleep_mode()
                break

            # Думаем
            if arduino.connected:
                arduino.mouth_closed()

            # Ответ AI
            answer = ai.ask(text)

            # Озвучка (анимация через callback)
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
        print("[ЖОПА] Пока!")


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
