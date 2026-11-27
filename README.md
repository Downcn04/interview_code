# 会议 / 面试 AI 助手（共享屏幕不可见）

实时把麦克风或会议声音转成文字，交给大模型，在悬浮小窗显示回答。窗口使用 Windows
`SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`，在屏幕共享 / 录屏 / 截图中对观众不可见，
仅本机可见；支持拖动、半透明、最小化。

> 本项目**仅使用阿里云智能语音交互（实时语音识别）**，请先开通并配置阿里云，再运行。

## 功能

- 阿里云实时语音识别（WebSocket 流式，中文 / 英文）
- 大模型：任意 OpenAI 兼容接口（DeepSeek / OpenAI / Ollama / vLLM …）
- 滚动会话记忆：保留最近 N 轮问答（默认 8 轮）
- 面试化系统提示：独立 `system_prompt.txt`（支持多行），可按岗位方向作答
- 可选简历 / JD 注入：`resume.md`、`jd.md`（存在才加载）
- 悬浮窗：无边框、置顶、半透明、可拖动、可最小化成细条、共享不可见

## 技术栈

- C++17 + Qt6（Widgets / Multimedia / Network / WebSockets）
- 构建：CMake + Ninja + MinGW 13.1
- 共享不可见：Win32 `SetWindowDisplayAffinity`

## 目录结构

```
.
├── CMakeLists.txt
├── build.ps1             # 一键构建脚本
├── .env.example          # 配置模板（复制为 .env）
├── system_prompt.txt     # 面试化系统提示（存在则优先使用）
└── src/
    ├── main.cpp          # 入口（含 --test-token / --test-asr 自检）
    ├── config.h/.cpp     # 读取 .env / 环境变量 / 提示词文件
    ├── mainwindow.h/.cpp # 悬浮窗 UI、共享不可见、最小化
    ├── asrbackend.h      # ASR 抽象接口
    ├── aliyunasr.h/.cpp  # 阿里云实时识别（换 Token + WebSocket）
    └── aiclient.h/.cpp   # 大模型调用 + 会话记忆
```

## 环境要求

- Qt 6.7.3（win64_mingw，需含 `qtmultimedia`、`qtwebsockets`）
- MinGW 13.1 工具链
- CMake + Ninja
- 阿里云智能语音交互账号（必须）
- Windows 10 1809（build 17763）或更高

## 阿里云准备（必须）

1. 控制台开通「智能语音交互 → 实时语音识别」
2. 创建项目，获取 **AppKey**
3. 创建 **AccessKey ID / Secret**
4. 以上必须属于同一个阿里云账号；程序会用 AccessKey 自动换取 Token

## 配置

复制 `.env.example` 为 `.env`，按文件内注释填写大模型与阿里云配置。

## 构建

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

产物：`build\MeetingAI.exe`。

## 运行

```powershell
$env:Path = "C:\Qt\6.7.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;" + $env:Path
.\build\MeetingAI.exe
```

自检：

```powershell
.\build\MeetingAI.exe --test-token   # 验证 AccessKey 能否换取 Token
.\build\MeetingAI.exe --test-asr     # 验证 AppKey 与实时识别服务是否可用
```

## 说明

- 若需捕获会议软件里对方的声音，把系统录音设备设为「立体声混音」，或后续接 WASAPI Loopback。
- 未配置 `AI_API_KEY` 时以模拟模式运行，便于先验证界面与识别。
- `.env`、模型、二进制均不入库，请本地准备。
