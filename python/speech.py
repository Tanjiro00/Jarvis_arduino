"""Модуль распознавания речи — OpenAI Whisper с адаптивным определением тишины."""

import os
import struct
import tempfile
import time
import wave

import pyaudio
from openai import OpenAI

import config


class SpeechRecognizer:
    """Запись с микрофона + распознавание через OpenAI Whisper API."""

    def __init__(self, client=None, language="ru"):
        self.client = client or OpenAI(api_key=config.OPENAI_API_KEY)
        self.language = language

        self.sample_rate = config.SAMPLE_RATE
        self.channels = config.CHANNELS
        self.chunk = config.CHUNK
        self.format = pyaudio.paInt16

        self.silence_threshold = config.MIN_SILENCE_THRESHOLD
        self.max_record_sec = config.MAX_RECORD_SEC

        self.pa = pyaudio.PyAudio()

    def calibrate(self, duration=1):
        """Калибровка шумового фона."""
        print("[Mic] Калибровка...")
        stream = self.pa.open(
            format=self.format, channels=self.channels,
            rate=self.sample_rate, input=True,
            frames_per_buffer=self.chunk
        )
        levels = []
        chunks_needed = int(self.sample_rate / self.chunk * duration)
        for _ in range(chunks_needed):
            data = stream.read(self.chunk, exception_on_overflow=False)
            levels.append(self._rms(data))
        stream.stop_stream()
        stream.close()

        avg_noise = sum(levels) / len(levels) if levels else 300
        self.silence_threshold = max(int(avg_noise * 1.8), config.MIN_SILENCE_THRESHOLD)
        print(f"[Mic] Порог тишины: {self.silence_threshold}")

    def listen(self, timeout=None, phrase_time_limit=None):
        """
        Слушает микрофон с адаптивным определением конца фразы.
        Возвращает текст или None.
        """
        timeout = timeout or config.LISTEN_TIMEOUT
        stream = self.pa.open(
            format=self.format, channels=self.channels,
            rate=self.sample_rate, input=True,
            frames_per_buffer=self.chunk
        )

        frames = []
        silent_chunks = 0
        has_speech = False
        speech_chunks = 0
        max_chunks = int(self.sample_rate / self.chunk * self.max_record_sec)
        timeout_chunks = int(self.sample_rate / self.chunk * timeout)
        waited = 0

        print("[Mic] Слушаю...")

        try:
            # Фаза 1: ждём начала речи
            while waited < timeout_chunks:
                data = stream.read(self.chunk, exception_on_overflow=False)
                level = self._rms(data)
                if level > self.silence_threshold:
                    has_speech = True
                    frames.append(data)
                    speech_chunks = 1
                    break
                waited += 1

            if not has_speech:
                print("[Mic] Тишина — никто не говорит")
                stream.stop_stream()
                stream.close()
                return None

            # Фаза 2: запись с адаптивным порогом тишины
            for _ in range(max_chunks):
                data = stream.read(self.chunk, exception_on_overflow=False)
                frames.append(data)
                level = self._rms(data)

                if level >= self.silence_threshold:
                    silent_chunks = 0
                    speech_chunks += 1
                else:
                    silent_chunks += 1
                    # Адаптивный порог тишины
                    silence_limit = self._adaptive_silence_limit(speech_chunks)
                    if silent_chunks >= silence_limit:
                        break

        finally:
            stream.stop_stream()
            stream.close()

        if not frames:
            return None

        speech_duration = speech_chunks * self.chunk / self.sample_rate
        print(f"[Mic] Записано {speech_duration:.1f} сек")

        # Сохраняем WAV
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            tmp_path = f.name
            wf = wave.open(f, "wb")
            wf.setnchannels(self.channels)
            wf.setsampwidth(self.pa.get_sample_size(self.format))
            wf.setframerate(self.sample_rate)
            wf.writeframes(b"".join(frames))
            wf.close()

        # Отправляем в Whisper с retry
        return self._transcribe_with_retry(tmp_path)

    def close(self):
        self.pa.terminate()

    # === Внутренние методы ===

    def _adaptive_silence_limit(self, speech_chunks):
        """Адаптивный порог тишины в chunks."""
        speech_sec = speech_chunks * self.chunk / self.sample_rate

        if speech_sec < 3.0:
            silence_sec = config.SILENCE_SHORT_PHRASE
        elif speech_sec > 5.0:
            silence_sec = config.SILENCE_LONG_PHRASE
        else:
            silence_sec = config.SILENCE_DURATION

        return int(self.sample_rate / self.chunk * silence_sec)

    def _transcribe_with_retry(self, audio_path, max_retries=2):
        """Транскрибация через Whisper API с retry."""
        for attempt in range(max_retries):
            try:
                with open(audio_path, "rb") as audio_file:
                    result = self.client.audio.transcriptions.create(
                        model=config.WHISPER_MODEL,
                        file=audio_file,
                        language=self.language,
                    )
                text = result.text.strip()
                if text:
                    print(f"[Mic] Распознано: {text}")
                    return text
                return None
            except Exception as e:
                print(f"[Mic] Ошибка Whisper (попытка {attempt + 1}): {e}")
                if attempt < max_retries - 1:
                    time.sleep(1)
            finally:
                if attempt == max_retries - 1:
                    try:
                        os.unlink(audio_path)
                    except Exception:
                        pass

        print("[Mic] Whisper не отвечает")
        return None

    @staticmethod
    def _rms(data):
        """Среднеквадратичная амплитуда."""
        count = len(data) // 2
        if count == 0:
            return 0
        shorts = struct.unpack(f"{count}h", data)
        sum_sq = sum(s * s for s in shorts)
        return int((sum_sq / count) ** 0.5)
