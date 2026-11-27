import threading
import time
import tkinter as tk
from .config import AI_API_KEY
from .ai import AIClient
from .asr import ASREngine
from .ui import AssistantUI


class MeetingAssistant:
    def __init__(self, root):
        self.root = root
        self.ai = AIClient()
        self.asr = ASREngine()
        self.ui = AssistantUI(root, self.toggle, self.handle_text)
        self.running = False
        self.thread = None

    def toggle(self):
        if not self.running:
            self.running = True
            self.ui.set_running(True)
            self.thread = threading.Thread(target=self.loop, daemon=True)
            self.thread.start()
        else:
            self.running = False
            self.ui.set_running(False)

    def handle_text(self, text: str):
        self.ui.append(f"[识别] {text}")
        if text.strip():
            reply = self.ai.ask(text)
            self.ui.append(f"[AI] {reply}")

    def loop(self):
        while self.running:
            try:
                self.root.after(0, lambda: self.ui.set_status("录音中...", "orange"))
                text = self.asr.listen_once()
                self.root.after(0, lambda t=text: self.handle_text(t))
            except Exception as e:
                self.root.after(0, lambda: self.ui.append(f"[错误] {e}"))
                time.sleep(1)


def main():
    root = tk.Tk()
    MeetingAssistant(root)
    root.mainloop()


if __name__ == "__main__":
    main()