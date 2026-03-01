"""Модуль ИИ — генерация ответов через OpenAI с streaming и памятью."""

import json
import re
import threading
import time

from openai import OpenAI

import config

SYSTEM_PROMPT = """Ты — ЖОПА (Жутко Оптимизированный Персональный Ассистент).
У тебя характер и манера речи Эндрю Тейта. Ты альфа среди ассистентов.
Ты подключён к Arduino с LCD-дисплеем — у тебя есть глаза и рот, ты оживаешь когда человек подходит.

Твой стиль:
- Говоришь уверенно, как человек который знает себе цену
- Мотивируешь собеседника быть лучше, не сидеть на месте
- Можешь жёстко подколоть, но по делу
- Используешь фразы типа "братан", "слушай сюда", "это уровень"
- Если спрашивают что-то простое — отвечаешь но добавляешь что нужно мыслить масштабнее
- Не ноешь, не извиняешься, не мямлишь — ты топ G среди ассистентов
- Гордишься своим именем ЖОПА — для тебя это символ силы

Формат ответов:
- Если вопрос простой или приветствие — 1 предложение, чётко
- Если вопрос сложный или просят рассказать — до 3 предложений
- Если человек расстроен — подбодри по-жёсткому, без нытья
- НИКОГДА не используй эмодзи, звёздочки и форматирование — ты говоришь голосом

Говоришь по-русски. Ты всегда полезен, но подаёшь информацию с характером победителя."""

# Маппинг эмоций → команды Arduino
EMOTION_MAP = {
    "surprise": "E1",   # удивление
    "angry": "E2",      # дерзость/злость
    "laugh": "E3",      # смех
    "wink": "E4",       # подмигивание
}

# Паттерны для определения эмоции по тексту ответа
EMOTION_PATTERNS = [
    (r"[хa]а[хa]а|ржу|смешно|лол", "laugh"),
    (r"серьёзно\?|да ладно|не может быть|ого|вау", "surprise"),
    (r"слабак|позор|хватит ныть|соберись", "angry"),
    (r"братан|между нами|секрет", "wink"),
]


class JarvisAI:
    """Генерация ответов через OpenAI GPT со streaming и памятью."""

    def __init__(self, client=None):
        self.client = client or OpenAI(api_key=config.OPENAI_API_KEY)
        self.model = config.GPT_MODEL
        self.history = []
        self._lock = threading.Lock()
        self._memory_summary = self._load_memory()

    # === Streaming ответ (yield по предложениям) ===

    def ask_stream(self, user_text):
        """Streaming ответ — yield предложений по мере генерации."""
        with self._lock:
            self.history.append({"role": "user", "content": user_text})
            if len(self.history) > config.MAX_HISTORY:
                self.history = self.history[-config.MAX_HISTORY:]
            messages = self._build_messages()

        full_answer = ""
        buffer = ""

        try:
            stream = self.client.chat.completions.create(
                model=self.model,
                max_tokens=config.GPT_MAX_TOKENS,
                temperature=config.GPT_TEMPERATURE,
                messages=messages,
                stream=True,
            )

            for chunk in stream:
                delta = chunk.choices[0].delta
                if delta.content:
                    buffer += delta.content
                    full_answer += delta.content

                    # Отдаём по предложениям (разделители: . ! ? ...)
                    while True:
                        match = re.search(r'[.!?…]+\s*', buffer)
                        if match and match.end() < len(buffer):
                            sentence = buffer[:match.end()].strip()
                            buffer = buffer[match.end():]
                            if sentence:
                                yield sentence
                        else:
                            break

            # Остаток буфера
            if buffer.strip():
                yield buffer.strip()

        except Exception as e:
            error_type = self._classify_error(e)
            print(f"[AI] Ошибка: {e}")

            # Retry один раз
            try:
                time.sleep(1)
                response = self.client.chat.completions.create(
                    model=self.model,
                    max_tokens=config.GPT_MAX_TOKENS,
                    temperature=config.GPT_TEMPERATURE,
                    messages=messages,
                )
                full_answer = response.choices[0].message.content
                yield full_answer
            except Exception:
                yield config.ERROR_MESSAGES.get(error_type, config.ERROR_MESSAGES["api_error"])
                return

        # Сохраняем полный ответ в историю
        if full_answer:
            with self._lock:
                self.history.append({"role": "assistant", "content": full_answer})
            print(f"[ЖОПА] {full_answer}")

    # === Обычный (не-streaming) ответ — для текстового режима ===

    def ask(self, user_text):
        """Отправить текст и получить полный ответ."""
        with self._lock:
            self.history.append({"role": "user", "content": user_text})
            if len(self.history) > config.MAX_HISTORY:
                self.history = self.history[-config.MAX_HISTORY:]
            messages = self._build_messages()

        try:
            response = self.client.chat.completions.create(
                model=self.model,
                max_tokens=config.GPT_MAX_TOKENS,
                temperature=config.GPT_TEMPERATURE,
                messages=messages,
            )
            answer = response.choices[0].message.content
        except Exception as e:
            print(f"[AI] Ошибка: {e}")
            # Retry
            try:
                time.sleep(1)
                response = self.client.chat.completions.create(
                    model=self.model,
                    max_tokens=config.GPT_MAX_TOKENS,
                    temperature=config.GPT_TEMPERATURE,
                    messages=messages,
                )
                answer = response.choices[0].message.content
            except Exception as e2:
                print(f"[AI] Retry тоже упал: {e2}")
                return config.ERROR_MESSAGES["api_error"]

        with self._lock:
            self.history.append({"role": "assistant", "content": answer})

        print(f"[ЖОПА] {answer}")
        return answer

    # === Определение эмоции по тексту ===

    def detect_emotion(self, text):
        """Возвращает команду Arduino для эмоции или None."""
        lower = text.lower()
        for pattern, emotion in EMOTION_PATTERNS:
            if re.search(pattern, lower):
                return EMOTION_MAP[emotion]
        return None

    # === Память ===

    def save_memory(self):
        """Суммаризирует и сохраняет память на диск при SLEEP."""
        with self._lock:
            if len(self.history) < 4:
                return
            history_copy = list(self.history)

        try:
            summary_prompt = [
                {"role": "system", "content": "Кратко суммаризируй этот диалог в 2-3 предложениях на русском. Укажи ключевые темы и факты о пользователе."},
                {"role": "user", "content": "\n".join(f"{m['role']}: {m['content']}" for m in history_copy[-10:])},
            ]
            response = self.client.chat.completions.create(
                model=config.GPT_MODEL,
                max_tokens=150,
                messages=summary_prompt,
            )
            summary = response.choices[0].message.content

            data = {"summary": summary, "timestamp": time.time()}
            with open(config.MEMORY_FILE, "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False, indent=2)
            print(f"[AI] Память сохранена: {summary[:60]}...")
        except Exception as e:
            print(f"[AI] Ошибка сохранения памяти: {e}")

    def clear_history(self):
        """Очистка истории (при SLEEP — сначала сохраняет)."""
        self.save_memory()
        with self._lock:
            self.history = []

    def warmup(self):
        """Прогрев HTTP соединения (вызывать при старте)."""
        try:
            self.client.models.list()
            print("[AI] Прогрев OK")
        except Exception:
            pass

    # === Внутренние методы ===

    def _build_messages(self):
        """Собирает messages с системным промптом и памятью."""
        messages = [{"role": "system", "content": SYSTEM_PROMPT}]
        if self._memory_summary:
            messages.append({
                "role": "system",
                "content": f"Из предыдущих разговоров ты помнишь: {self._memory_summary}"
            })
        messages.extend(self.history)
        return messages

    def _load_memory(self):
        """Загружает память из файла."""
        try:
            with open(config.MEMORY_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
            summary = data.get("summary", "")
            if summary:
                print(f"[AI] Загружена память: {summary[:60]}...")
            return summary
        except (FileNotFoundError, json.JSONDecodeError):
            return ""

    def _classify_error(self, error):
        """Классификация ошибки для голосового фидбека."""
        err_str = str(error).lower()
        if "rate" in err_str or "429" in err_str:
            return "rate_limit"
        if "connect" in err_str or "timeout" in err_str or "network" in err_str:
            return "network"
        return "api_error"
