import json
import speech_recognition as sr
from .config import (
    ASR_ENGINE,
    ASR_LANGUAGE,
    WHISPER_MODEL,
    WHISPER_DEVICE,
    WHISPER_COMPUTE_TYPE,
    VOSK_MODEL_PATH,
)


class ASREngine:
    def __init__(self):
        self.engine = ASR_ENGINE
        self.recognizer = sr.Recognizer()
        self.mic = sr.Microphone()
        with self.mic as source:
            self.recognizer.adjust_for_ambient_noise(source, duration=1)

        self._whisper = None
        self._vosk = None
        self._vosk_rec = None

        if self.engine == "whisper":
            from faster_whisper import WhisperModel
            self._whisper = WhisperModel(
                WHISPER_MODEL,
                device=WHISPER_DEVICE,
                compute_type=WHISPER_COMPUTE_TYPE,
            )
        elif self.engine == "vosk":
            from vosk import Model, KaldiRecognizer
            if not VOSK_MODEL_PATH:
                raise RuntimeError("VOSK_MODEL_PATH 未设置，请在 .env 中指定 vosk 模型目录")
            self._vosk_model = Model(VOSK_MODEL_PATH)

    def listen_once(self, timeout: int = 5, phrase_time_limit: int = 15) -> str:
        with self.mic as source:
            audio = self.recognizer.listen(
                source, timeout=timeout, phrase_time_limit=phrase_time_limit
            )

        if self.engine == "google":
            return self.recognizer.recognize_google(audio, language=ASR_LANGUAGE)

        if self.engine == "whisper":
            return self._transcribe_whisper(audio)

        if self.engine == "vosk":
            return self._transcribe_vosk(audio)

        raise ValueError(f"不支持的 ASR_ENGINE: {self.engine}")

    def _audio_to_float32(self, audio: sr.AudioData):
        import numpy as np

        raw = audio.get_raw_data(
            convert_rate=16000,
            convert_width=2,
        )
        return np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0

    def _transcribe_whisper(self, audio: sr.AudioData) -> str:
        samples = self._audio_to_float32(audio)
        segments, _ = self._whisper.transcribe(
            samples,
            language="zh" if ASR_LANGUAGE.startswith("zh") else None,
            vad_filter=True,
        )
        return "".join(seg.text for seg in segments).strip()

    def _transcribe_vosk(self, audio: sr.AudioData) -> str:
        from vosk import KaldiRecognizer
        import wave

        if self._vosk_rec is None:
            self._vosk_rec = KaldiRecognizer(self._vosk_model, 16000)

        raw = audio.get_raw_data(convert_rate=16000, convert_width=2)
        if self._vosk_rec.AcceptWaveform(raw):
            res = json.loads(self._vosk_rec.Result())
            return res.get("text", "").strip()
        res = json.loads(self._vosk_rec.FinalResult())
        return res.get("text", "").strip()
