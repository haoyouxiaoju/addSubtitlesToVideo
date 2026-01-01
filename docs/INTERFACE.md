# 接口文档 (Interface Documentation)

## 1. 概述
本项目主要由 C++ 主程序 (`VideoSubtitleGenerator.exe`) 和 Python 转录脚本 (`transcribe.py`) 组成。两者通过命令行参数（CLI）和标准输出（StdOut/StdErr）进行通信。

## 2. Python 脚本接口 (`transcribe.py`)

### 2.1 调用方式
C++ 程序通过 `QProcess` 调用 Python 解释器执行脚本。

```bash
python transcribe.py <input_wav> <output_srt> [options]
```

### 2.2 参数说明

| 参数 | 类型 | 必选 | 描述 |
| :--- | :--- | :--- | :--- |
| `input_wav` | Positional | 是 | 输入的 WAV 音频文件路径 (需 16kHz 单声道) |
| `output_srt` | Positional | 是 | 输出的 SRT 字幕文件路径 |
| `--engine` | Option | 否 | 转录引擎，可选 `vosk` (默认) 或 `whisper` |
| `--model` | Option | 否 | 模型名称 (仅 Whisper 有效)，可选 `tiny`, `base`, `small`, `medium`, `large` |

### 2.3 输出协议 (Stdout/Stderr)

为了实现进度条同步，Python 脚本会向标准输出打印特定的格式化字符串。

#### 2.3.1 进度信息 (Stdout)
C++ 程序通过解析 `sys.stdout` 捕获以下标签：

- **下载进度** (Whisper 模型下载):
  ```
  DOWNLOAD_PROGRESS: <percent>
  ```
  *示例*: `DOWNLOAD_PROGRESS: 45` (表示下载了 45%)

- **转录进度** (Whisper/Vosk 转录):
  ```
  TRANS_PROGRESS: <percent>
  ```
  *示例*: `TRANS_PROGRESS: 50` (表示转录了 50%)

#### 2.3.2 错误信息 (Stderr)
所有异常堆栈和错误日志输出到 `sys.stderr`，C++ 程序会将其捕获并显示在日志窗口中。

## 3. FFmpeg 接口

C++ 程序直接调用 FFmpeg 可执行文件进行音频处理和视频合成。

### 3.1 音频提取 (Stage 1)
```bash
ffmpeg -y -i <input_video> -ac 1 -ar 16000 -f wav <temp_audio.wav>
```
- **进度解析**: 通过 `stderr` 中的 `Duration: HH:MM:SS.ms` 获取总时长，`time=HH:MM:SS.ms` 获取当前进度。

### 3.2 视频合成 (Stage 3)
```bash
ffmpeg -y -i <input_video> -vf subtitles='<subs.srt>' -c:v libx264 -preset fast -c:a copy <output_video>
```
- **进度解析**: 同上，通过 `time` 字段计算合成进度。

## 4. 异常处理逻辑

### 4.1 Python 侧
- 捕获所有 `Exception`，打印 Traceback 到 stderr。
- 如果发生错误，脚本返回非零 Exit Code。
- 特定错误（如 GPU 显存不足）会尝试回退或给出明确提示。

### 4.2 C++ 侧
- 监听 `QProcess::finished` 信号，检查 Exit Code。
- 如果 Exit Code != 0，标记任务为失败，并显示红色状态。
- 监听 `readyReadStandardError`，如果发现 "Error" 关键字，记录到 UI 日志区。
