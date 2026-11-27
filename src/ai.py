import requests
from .config import AI_BASE_URL, AI_API_KEY, AI_MODEL


class AIClient:
    def __init__(self):
        self.base_url = AI_BASE_URL
        self.api_key = AI_API_KEY
        self.model = AI_MODEL

    def ask(self, text: str) -> str:
        if not self.api_key:
            return f"（未配置 API Key，模拟回答）你刚才说：「{text}」。"
        try:
            headers = {
                "Authorization": f"Bearer {self.api_key}",
                "Content-Type": "application/json",
            }
            data = {
                "model": self.model,
                "messages": [
                    {"role": "system", "content": "你是一个会议实时助手，请用简洁中文回答，直接给出要点。"},
                    {"role": "user", "content": text},
                ],
                "temperature": 0.7,
            }
            resp = requests.post(self.base_url, headers=headers, json=data, timeout=30)
            resp.raise_for_status()
            return resp.json()["choices"][0]["message"]["content"]
        except Exception as e:
            return f"[AI 错误] {e}"