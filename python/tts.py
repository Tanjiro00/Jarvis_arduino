"""Модуль синтеза речи — OpenAI TTS с поддержкой streaming chunks."""

import array
import math
import os
import tempfile
import time

from openai import OpenAI
import pygame

import config


class TextToSpeech:
    """Озвучка через OpenAI TTS API — живой человеческий голос."""

    def __init__(self, client=None, voice=None):
        self.client = client or OpenAI(api_key=config.OPENAI_API_KEY)
        self.voice = voice or config.TTS_VOICE
        self._on_start = None
        self._on_end = None
        self._speaking = False

        pygame.mixer.init(frequency=24000, size=-16, channels=1)

    def on_start(self, callback):
        self._on_start = callback

    def on_end(self, callback):
        self._on_end = callback

    # === Озвучка одного предложения (для streaming pipeline) ===

    def speak_chunk(self, text, is_first=False, is_last=False):
        """Озвучить одно предложение. Для streaming GPT → TTS."""
        if not text:
            return

        if is_first and self._on_start:
            self._on_start()
            self._speaking = True

        self._play_text(text)

        if is_last and self._on_end:
            self._on_end()
            self._speaking = False

    # === Полная озвучка (для текстового режима / ошибок) ===

    def speak(self, text):
        """Озвучить весь текст. Блокирующий вызов."""
        if not text:
            return

        if self._on_start:
            self._on_start()

        self._play_text(text)

        if self._on_end:
            self._on_end()

    def stop(self):
        """Остановить воспроизведение."""
        pygame.mixer.music.stop()
        if self._speaking and self._on_end:
            self._on_end()
            self._speaking = False

    # === Звуковые эффекты ===

    @staticmethod
    def play_beep(frequency=None, duration_ms=None, volume=None):
        """Короткий beep — подтверждение wake-word."""
        freq = frequency or config.BEEP_FREQUENCY
        dur = duration_ms or config.BEEP_DURATION
        vol = volume or config.BEEP_VOLUME

        sample_rate = 24000
        n_samples = int(sample_rate * dur / 1000)

        buf = array.array("h", [0] * n_samples)
        for i in range(n_samples):
            t = i / sample_rate
            # Fade in/out для мягкого звука
            envelope = 1.0
            fade = int(n_samples * 0.1)
            if i < fade:
                envelope = i / fade
            elif i > n_samples - fade:
                envelope = (n_samples - i) / fade
            buf[i] = int(32767 * vol * envelope * math.sin(2 * math.pi * freq * t))

        sound = pygame.mixer.Sound(buffer=buf)
        sound.play()
        pygame.time.wait(dur + 20)

    @staticmethod
    def play_melody(notes, duration_ms=150, volume=0.2):
        """Проиграть мелодию из нот. notes = список частот в Hz."""
        sample_rate = 24000

        for freq in notes:
            n_samples = int(sample_rate * duration_ms / 1000)
            buf = array.array("h", [0] * n_samples)
            for i in range(n_samples):
                t = i / sample_rate
                envelope = 1.0
                fade = int(n_samples * 0.15)
                if i < fade:
                    envelope = i / fade
                elif i > n_samples - fade:
                    envelope = (n_samples - i) / fade
                buf[i] = int(32767 * volume * envelope * math.sin(2 * math.pi * freq * t))

            sound = pygame.mixer.Sound(buffer=buf)
            sound.play()
            pygame.time.wait(duration_ms)

    @staticmethod
    def play_wake_sound():
        """Мелодия пробуждения — 3 ноты вверх."""
        TextToSpeech.play_melody([523, 659, 784], duration_ms=120, volume=0.2)

    @staticmethod
    def play_sleep_sound():
        """Мелодия засыпания — 3 ноты вниз."""
        TextToSpeech.play_melody([784, 659, 523], duration_ms=180, volume=0.15)

    # === Внутренние ===

    def _play_text(self, text):
        """Генерация и воспроизведение аудио для текста с retry."""
        for attempt in range(2):
            try:
                response = self.client.audio.speech.create(
                    model=config.TTS_MODEL,
                    voice=self.voice,
                    input=text,
                    response_format="mp3",
                )

                with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as f:
                    f.write(response.content)
                    tmp_path = f.name

                pygame.mixer.music.load(tmp_path)
                pygame.mixer.music.play()

                while pygame.mixer.music.get_busy():
                    pygame.time.wait(50)

                try:
                    pygame.mixer.music.unload()
                    os.unlink(tmp_path)
                except Exception:
                    pass

                return  # Успех

            except Exception as e:
                print(f"[TTS] Ошибка (попытка {attempt + 1}): {e}")
                if attempt == 0:
                    time.sleep(0.5)
                else:
                    print("[TTS] Не удалось озвучить")
