"""Модуль распознавания речи — OpenAI Whisper (точное распознавание)."""

import io
import os
import wave
import tempfile

import pyaudio
from openai import OpenAI


class SpeechRecognizer:
    """Запись с микрофона + распознавание через OpenAI Whisper API."""

    def __init__(self, language="ru", api_key=None):
        self.client = OpenAI(api_key=api_key or os.environ.get("OPENAI_API_KEY"))
        self.language = language

        # Параметры записи
        self.sample_rate = 16000
        self.channels = 1
        self.chunk = 1024
        self.format = pyaudio.paInt16

        # Порог тишины для автостопа
        self.silence_threshold = 500   # амплитуда
        self.silence_duration = 1.5    # секунд тишины = стоп
        self.max_record_sec = 15       # максимум записи

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
            level = self._rms(data)
            levels.append(level)
        stream.stop_stream()
        stream.close()

        avg_noise = sum(levels) / len(levels) if levels else 300
        self.silence_threshold = int(avg_noise * 1.8)
        if self.silence_threshold < 300:
            self.silence_threshold = 300
        print(f"[Mic] Порог тишины: {self.silence_threshold}")

    def _rms(self, data):
        """Среднеквадратичная амплитуда."""
        import struct
        count = len(data) // 2
        shorts = struct.unpack(f"{count}h", data)
        sum_sq = sum(s * s for s in shorts)
        return int((sum_sq / count) ** 0.5) if count > 0 else 0

    def listen(self, timeout=10, phrase_time_limit=15):
        """
        Слушает микрофон, записывает до тишины, отправляет в Whisper.
        Возвращает текст или None.
        """
        stream = self.pa.open(
            format=self.format, channels=self.channels,
            rate=self.sample_rate, input=True,
            frames_per_buffer=self.chunk
        )

        frames = []
        silent_chunks = 0
        has_speech = False
        max_chunks = int(self.sample_rate / self.chunk * self.max_record_sec)
        timeout_chunks = int(self.sample_rate / self.chunk * timeout)
        silence_limit = int(self.sample_rate / self.chunk * self.silence_duration)
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
                    break
                waited += 1

            if not has_speech:
                print("[Mic] Тишина — никто не говорит")
                stream.stop_stream()
                stream.close()
                return None

            # Фаза 2: пишем до тишины
            for _ in range(max_chunks):
                data = stream.read(self.chunk, exception_on_overflow=False)
                frames.append(data)
                level = self._rms(data)
                if level < self.silence_threshold:
                    silent_chunks += 1
                    if silent_chunks >= silence_limit:
                        break
                else:
                    silent_chunks = 0

        finally:
            stream.stop_stream()
            stream.close()

        if not frames:
            return None

        # Сохраняем WAV во временный файл
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            tmp_path = f.name
            wf = wave.open(f, "wb")
            wf.setnchannels(self.channels)
            wf.setsampwidth(self.pa.get_sample_size(self.format))
            wf.setframerate(self.sample_rate)
            wf.writeframes(b"".join(frames))
            wf.close()

        # Отправляем в Whisper
        try:
            with open(tmp_path, "rb") as audio_file:
                result = self.client.audio.transcriptions.create(
                    model="whisper-1",
                    file=audio_file,
                    language=self.language,
                )
            text = result.text.strip()
            if text:
                print(f"[Mic] Распознано: {text}")
                return text
            return None
        except Exception as e:
            print(f"[Mic] Ошибка Whisper: {e}")
            return None
        finally:
            try:
                os.unlink(tmp_path)
            except Exception:
                pass

    def close(self):
        self.pa.terminate()
