"""Модуль ИИ — генерация ответов через OpenAI."""

import os
from openai import OpenAI

SYSTEM_PROMPT = """Ты — ЖОПА (Жутко Оптимизированный Персональный Ассистент).
Ты дерзкий, остроумный, но полезный голосовой помощник.
Говоришь по-русски. Отвечаешь кратко — 1-3 предложения.
Ты не стесняешься своего имени — наоборот, гордишься им.
У тебя есть характер: можешь подколоть, пошутить, но всегда помогаешь.
Если тебя спрашивают как тебя зовут — отвечаешь с гордостью.
Не извиняйся, не мямли. Будь конкретным и полезным."""

MAX_HISTORY = 20


class JarvisAI:
    """Генерация ответов через OpenAI GPT."""

    def __init__(self, api_key=None, model="gpt-4o-mini"):
        self.client = OpenAI(api_key=api_key or os.environ.get("OPENAI_API_KEY"))
        self.model = model
        self.history = []

    def ask(self, user_text):
        """Отправить текст и получить ответ."""
        self.history.append({"role": "user", "content": user_text})

        if len(self.history) > MAX_HISTORY:
            self.history = self.history[-MAX_HISTORY:]

        messages = [{"role": "system", "content": SYSTEM_PROMPT}] + self.history

        response = self.client.chat.completions.create(
            model=self.model,
            max_tokens=300,
            temperature=0.9,
            messages=messages,
        )

        answer = response.choices[0].message.content
        self.history.append({"role": "assistant", "content": answer})

        print(f"[ЖОПА] {answer}")
        return answer

    def clear_history(self):
        self.history = []
