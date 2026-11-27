# Vosk C++ SDK（第三方）

本目录只提交了 `include/vosk_api.h`，**二进制不入库**。构建前请自行下载 Vosk Windows 64 位 SDK，并把文件放到：

```
cpp/third_party/vosk/lib/libvosk.dll
cpp/third_party/vosk/lib/libvosk.dll.a   # 导入库
```

没有 `libvosk.dll.a` 时，可用 `gendef` + `dlltool` 从 `libvosk.dll` 生成。

- SDK 下载：https://alphacephei.com/vosk/install
- 中文模型：https://alphacephei.com/vosk/models （如 `vosk-model-small-cn-0.22`）

> 仅当 `ASR_ENGINE=vosk` 时才需要以上文件；使用阿里云实时识别可不放。
