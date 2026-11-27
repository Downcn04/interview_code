import os
from pathlib import Path

from dotenv import load_dotenv

load_dotenv(Path(__file__).resolve().parent.parent / ".env")

AI_BASE_URL = os.getenv("AI_BASE_URL", "https://api.openai.com/v1/chat/completions")
AI_API_KEY = os.getenv("AI_API_KEY", "")
AI_MODEL = os.getenv("AI_MODEL", "gpt-3.5-turbo")

ASR_ENGINE = os.getenv("ASR_ENGINE", "google")
ASR_LANGUAGE = os.getenv("ASR_LANGUAGE", "zh-CN")

WHISPER_MODEL = os.getenv("WHISPER_MODEL", "base")
WHISPER_DEVICE = os.getenv("WHISPER_DEVICE", "cpu")
WHISPER_COMPUTE_TYPE = os.getenv("WHISPER_COMPUTE_TYPE", "int8")

VOSK_MODEL_PATH = os.getenv("VOSK_MODEL_PATH", "")

WINDOW_TITLE = os.getenv("WINDOW_TITLE", "会议 AI 助手（共享时不可见）")
WINDOW_WIDTH = int(os.getenv("WINDOW_WIDTH", "520"))
WINDOW_HEIGHT = int(os.getenv("WINDOW_HEIGHT", "420"))
WINDOW_X = int(os.getenv("WINDOW_X", "100"))
WINDOW_Y = int(os.getenv("WINDOW_Y", "100"))
