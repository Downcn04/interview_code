import tkinter as tk
from tkinter import scrolledtext
from .windows import make_unshareable
from .config import WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_X, WINDOW_Y


class AssistantUI:
    def __init__(self, root, on_toggle, on_text):
        self.root = root
        self.on_toggle = on_toggle
        self.on_text = on_text
        self._build()
        root.after(200, lambda: make_unshareable(root))

    def _build(self):
        self.root.title(WINDOW_TITLE)
        self.root.geometry(f"{WINDOW_WIDTH}x{WINDOW_HEIGHT}+{WINDOW_X}+{WINDOW_Y}")
        self.root.attributes("-topmost", True)
        self.root.configure(bg="#e3f2fd")

        ctrl = tk.Frame(self.root, bg="#e3f2fd")
        ctrl.pack(fill="x", padx=8, pady=6)

        self.btn = tk.Button(ctrl, text="开始监听", command=self.on_toggle,
                             bg="#2196f3", fg="white", width=12)
        self.btn.pack(side="left")

        self.status = tk.Label(ctrl, text="就绪", bg="#e3f2fd", fg="#666")
        self.status.pack(side="left", padx=10)

        self.log = scrolledtext.ScrolledText(self.root, wrap=tk.WORD, font=("微软雅黑", 11))
        self.log.pack(expand=True, fill="both", padx=8, pady=6)

    def set_running(self, running: bool):
        self.btn.config(text="停止监听" if running else "开始监听",
                        bg="#f44336" if running else "#2196f3")
        self.status.config(text="监听中..." if running else "已停止",
                           fg="green" if running else "gray")

    def set_status(self, text: str, color: str = "#666"):
        self.status.config(text=text, fg=color)

    def append(self, text: str):
        self.log.insert(tk.END, text + "\n")
        self.log.see(tk.END)