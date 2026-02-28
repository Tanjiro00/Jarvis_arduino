"""Модуль распознавания речи."""

import speech_recognition as sr


class SpeechRecognizer:
    """Распознавание речи через микрофон с Google Speech API."""

    def __init__(self, language="ru-RU"):
        self.recognizer = sr.Recognizer()
        self.language = language
        # Настройка чувствительности
        self.recognizer.energy_threshold = 300
        self.recognizer.dynamic_energy_threshold = True
        self.recognizer.pause_threshold = 1.0

    def calibrate(self, duration=1):
        """Калибровка шумового фона."""
        with sr.Microphone() as source:
            print("[Speech] Калибровка микрофона...")
            self.recognizer.adjust_for_ambient_noise(source, duration=duration)
            print(f"[Speech] Порог энергии: {self.recognizer.energy_threshold:.0f}")

    def listen(self, timeout=10, phrase_time_limit=15):
        """
        Слушает микрофон и возвращает распознанный текст.
        Возвращает None если не удалось распознать.
        """
        with sr.Microphone() as source:
            print("[Speech] Слушаю...")
            try:
                audio = self.recognizer.listen(
                    source,
                    timeout=timeout,
                    phrase_time_limit=phrase_time_limit
                )
            except sr.WaitTimeoutError:
                print("[Speech] Таймаут — тишина")
                return None

        return self._recognize(audio)

    def _recognize(self, audio):
        """Распознавание аудио через Google Speech API."""
        try:
            text = self.recognizer.recognize_google(audio, language=self.language)
            print(f"[Speech] Распознано: {text}")
            return text
        except sr.UnknownValueError:
            print("[Speech] Не удалось распознать речь")
            return None
        except sr.RequestError as e:
            print(f"[Speech] Ошибка API: {e}")
            return None
