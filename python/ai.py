"""Модуль взаимодействия с OpenAI API."""

import os
from openai import OpenAI

SYSTEM_PROMPT = (
    "Ты — JOPA (Just Optimized Personal Assistant), умный и лаконичный ИИ-ассистент. "
    "Отвечай кратко, по-русски, 1-3 предложения. "
    "Будь полезным, дружелюбным и немного ироничным."
)

MAX_HISTORY = 20  # максимум сообщений в истории (10 пар)


class JarvisAI:
    """Генерация ответов через OpenAI API."""

    def __init__(self, api_key=None):
        self.client = OpenAI(api_key=api_key or os.environ.get("OPENAI_API_KEY"))
        self.history = []

    def ask(self, user_text):
        """
        Отправить текст пользователя и получить ответ.
        Возвращает строку с ответом.
        """
        self.history.append({"role": "user", "content": user_text})

        # Обрезаем историю
        if len(self.history) > MAX_HISTORY:
            self.history = self.history[-MAX_HISTORY:]

        messages = [{"role": "system", "content": SYSTEM_PROMPT}] + self.history

        response = self.client.chat.completions.create(
            model="gpt-4o-mini",
            max_tokens=300,
            messages=messages,
        )

        answer = response.choices[0].message.content
        self.history.append({"role": "assistant", "content": answer})

        print(f"[AI] Ответ: {answer}")
        return answer

    def clear_history(self):
        """Очистить историю диалога."""
        self.history = []
