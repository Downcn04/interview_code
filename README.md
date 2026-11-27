# 会议 / 面试 AI 助手（共享屏幕不可见）

实时把会议或面试中的声音转成文字，交给大模型，在悬浮小窗显示回答。窗口基于 Windows
`SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`，在屏幕共享 / 录屏 / 截图中对观众不可见，
仅本机可见。

> 本项目**仅使用阿里云智能语音交互（实时语音识别）**，请先开通并配置阿里云，再运行。

## 功能

- 阿里云实时语音识别（WebSocket 流式识别，中文 / 英文）
- 任意 OpenAI 兼容大模型（DeepSeek / OpenAI / Ollama …）
- 滚动会话记忆（默认最近 8 轮）+ 面试化系统提示，可注入简历 / JD
- 悬浮窗：无边框、置顶、半透明、可拖动、可最小化、共享不可见

## 目录

```
cpp/    # 主程序（C++ / Qt6），使用阿里云实时识别 —— 推荐
src/    # 早期 Python 原型（已废弃，仅供参考，不支持阿里云）
```

## 前置要求（必须）

1. 阿里云账号，开通「智能语音交互 → 实时语音识别」
2. 在控制台创建项目，获取 **AppKey**
3. 创建 **AccessKey ID / Secret**
4. AppKey、AccessKey、Token 必须属于同一个阿里云账号

## 快速开始

```powershell
cd cpp
copy .env.example .env
# 编辑 .env：填 AI_API_KEY、ALIYUN_APPKEY、ALIYUN_ACCESS_KEY_ID、ALIYUN_ACCESS_KEY_SECRET
powershell -ExecutionPolicy Bypass -File .\build.ps1

$env:Path = "C:\Qt\6.7.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;" + $env:Path
.\build\MeetingAI.exe
```

自检：

```powershell
.\build\MeetingAI.exe --test-token   # 验证 AccessKey 换 Token
.\build\MeetingAI.exe --test-asr     # 验证 AppKey 与实时服务
```

## 配置

详见 `cpp/.env.example` 与 `cpp/README.md`。系统提示在 `cpp/system_prompt.txt`；
可选放 `cpp/resume.md`、`cpp/jd.md` 做简历 / JD 注入。

## 说明

- 需要 Windows 10 1809（build 17763）或更高版本。
- 未配置 `AI_API_KEY` 时以模拟模式运行。
- 如需捕获会议里对方的声音，把系统录音设备设为「立体声混音」。
