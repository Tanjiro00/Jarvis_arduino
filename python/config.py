"""Единая конфигурация ЖОПА."""

import os

# === OpenAI ===
OPENAI_API_KEY = os.environ.get("OPENAI_API_KEY")
GPT_MODEL = "gpt-4o-mini"
GPT_MAX_TOKENS = 300
GPT_TEMPERATURE = 0.9
TTS_MODEL = "tts-1"
TTS_VOICE = "onyx"
WHISPER_MODEL = "whisper-1"

# === Wake-word ===
WAKE_WORDS = ["жопа", "жопу", "жоп", "zhopa"]

# === Речь ===
SAMPLE_RATE = 16000
CHANNELS = 1
CHUNK = 1024
SILENCE_DURATION = 1.5       # сек тишины = стоп (базовое значение)
SILENCE_SHORT_PHRASE = 1.2   # для коротких фраз (< 3 сек)
SILENCE_LONG_PHRASE = 2.0    # для длинных фраз (> 5 сек)
MAX_RECORD_SEC = 15
LISTEN_TIMEOUT = 10
MIN_SILENCE_THRESHOLD = 300

# === Разговор ===
MAX_HISTORY = 20
SUMMARY_THRESHOLD = 15       # при достижении — суммаризировать
MEMORY_FILE = os.path.expanduser("~/.zhopa_memory.json")

# === Arduino ===
BAUD_RATE = 9600
WAKE_DISTANCE_CM = 80
SLEEP_TIMEOUT_SEC = 12

# === Приветствия при WAKE ===
WAKE_GREETINGS = [
    "Ну что, братан?",
    "Слушаю!",
    "Давай, чего хотел?",
    "Я тут, братан!",
    "Топ G на связи!",
    "Говори, я весь внимание!",
]

# === Ошибки (фразы для голосового фидбека) ===
ERROR_MESSAGES = {
    "network": "Братан, связь потерялась. Жди пока вернётся.",
    "rate_limit": "Подожди секунду, слишком быстро работаем.",
    "api_error": "Что-то пошло не так, спроси ещё раз.",
    "whisper_error": "Не расслышал, братан. Повтори погромче.",
}

# === Beep ===
BEEP_FREQUENCY = 660    # Hz
BEEP_DURATION = 120     # ms
BEEP_VOLUME = 0.3
