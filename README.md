# 会议 AI 助手（共享屏幕不可见）

会议中自动识别对方发言，喂给大模型，实时在小窗显示回答。窗口使用 Windows `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`，在 Teams/Zoom/OBS 屏幕共享和截图中显示为空白，只有你自己能看见。

## 目录结构

```
ai_meeting_assistant/
├── src/
│   ├── __init__.py
│   ├── main.py       # 入口
│   ├── config.py     # 配置
│   ├── windows.py    # Windows API：共享时不可见
│   ├── ai.py         # 大模型调用（OpenAI 兼容接口）
│   ├── asr.py        # 语音识别（google / faster-whisper / vosk）
│   └── ui.py         # tkinter 界面
├── requirements.txt
├── .env.example
└── README.md
```

## 快速开始

1. 创建虚拟环境并安装依赖：

```powershell
cd C:\Users\maofe\Desktop\ai_meeting_assistant
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

> 若 `PyAudio` 安装失败，可改用 `pipwin`：`pip install pipwin && pipwin install pyaudio`

2. 配置环境变量：

```powershell
copy .env.example .env
# 编辑 .env 填入 AI_API_KEY、ASR_ENGINE 等
```

3. 运行：

```powershell
python -m src.main
```

## ASR 引擎选择

在 `.env` 中设置 `ASR_ENGINE`：

- `google`：Google Web Speech API，免费无需 Key，有配额，境外服务（默认）
- `whisper`：本地 `faster-whisper` 离线识别，需下载模型，完全本地运行
- `vosk`：本地 `Vosk` 离线识别，需手动下载模型文件并设置 `VOSK_MODEL_PATH`

### 使用 faster-whisper

```powershell
# .env
ASR_ENGINE=whisper
WHISPER_MODEL=base
WHISPER_DEVICE=cpu
WHISPER_COMPUTE_TYPE=int8
```

首次运行会自动下载 Whisper 模型（约 140MB base 模型），需联网。

### 使用 Vosk

1. 从 https://alphacephei.com/vosk/models 下载中文模型（如 `vosk-model-cn-0.22`）
2. 解压到任意目录，例如 `D:\models\vosk-model-cn-0.22`
3. `.env` 配置：

```powershell
ASR_ENGINE=vosk
VOSK_MODEL_PATH=D:\models\vosk-model-cn-0.22
```

## 注意事项

- 默认读取系统默认麦克风。如需捕获会议软件中对方的声音，请在 Windows 声音设置中将录音设备改为“立体声混音”，或使用 WASAPI Loopback 捕获系统输出。
- 共享屏幕不可见需 Windows 10 1809（build 17763）或更高版本。
- 未配置 `AI_API_KEY` 时会以模拟模式运行，方便先跑通界面。
