"""Модуль синтеза речи — OpenAI TTS (качественный голос)."""

import io
import os
import tempfile
import threading

from openai import OpenAI

# pygame для воспроизведения аудио — легче и надёжнее чем pyttsx3
import pygame


class TextToSpeech:
    """Озвучка через OpenAI TTS API — живой человеческий голос."""

    def __init__(self, voice="onyx", api_key=None):
        """
        voice: "onyx" (низкий мужской), "alloy", "echo", "fable", "nova", "shimmer"
        """
        self.client = OpenAI(api_key=api_key or os.environ.get("OPENAI_API_KEY"))
        self.voice = voice
        self._on_start = None
        self._on_end = None

        # Инициализация pygame mixer
        pygame.mixer.init(frequency=24000, size=-16, channels=1)

    def on_start(self, callback):
        self._on_start = callback

    def on_end(self, callback):
        self._on_end = callback

    def speak(self, text):
        """Озвучить текст. Блокирующий вызов."""
        if not text:
            return

        print(f"[TTS] Генерирую речь...")

        try:
            response = self.client.audio.speech.create(
                model="tts-1",
                voice=self.voice,
                input=text,
                response_format="mp3",
            )

            # Сохраняем во временный файл
            with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as f:
                f.write(response.content)
                tmp_path = f.name

            # Воспроизводим
            if self._on_start:
                self._on_start()

            pygame.mixer.music.load(tmp_path)
            pygame.mixer.music.play()

            # Ждём окончания
            while pygame.mixer.music.get_busy():
                pygame.time.wait(50)

            if self._on_end:
                self._on_end()

            # Удаляем временный файл
            try:
                pygame.mixer.music.unload()
                os.unlink(tmp_path)
            except Exception:
                pass

        except Exception as e:
            print(f"[TTS] Ошибка: {e}")
            if self._on_end:
                self._on_end()
