"""Модуль синтеза речи (Text-to-Speech)."""

import pyttsx3


class TextToSpeech:
    """Озвучивание текста через pyttsx3 с callback'ами для анимации."""

    def __init__(self, rate=150, volume=1.0):
        self.engine = pyttsx3.init()
        self.engine.setProperty("rate", rate)
        self.engine.setProperty("volume", volume)
        self._on_start = None
        self._on_end = None

        # Попробовать выбрать русский голос
        voices = self.engine.getProperty("voices")
        for voice in voices:
            if "ru" in voice.id.lower() or "russian" in voice.name.lower():
                self.engine.setProperty("voice", voice.id)
                print(f"[TTS] Голос: {voice.name}")
                break

        # Регистрация callback'ов движка
        self.engine.connect("started-utterance", self._handle_start)
        self.engine.connect("finished-utterance", self._handle_end)

    def on_start(self, callback):
        """Callback при начале произнесения."""
        self._on_start = callback

    def on_end(self, callback):
        """Callback при окончании произнесения."""
        self._on_end = callback

    def _handle_start(self, name):
        if self._on_start:
            self._on_start()

    def _handle_end(self, name, completed):
        if self._on_end:
            self._on_end()

    def speak(self, text):
        """Произнести текст. Блокирующий вызов."""
        print(f"[TTS] Говорю: {text}")
        self.engine.say(text)
        self.engine.runAndWait()

    def list_voices(self):
        """Показать все доступные голоса."""
        voices = self.engine.getProperty("voices")
        for v in voices:
            print(f"  {v.id} — {v.name} [{','.join(v.languages)}]")
