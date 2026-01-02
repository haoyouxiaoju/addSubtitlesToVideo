# 视频字幕生成器开发指南

本文档旨在帮助编程初学者理解本项目的实现原理，并指导如何从零开始完成类似的项目。

## 1. 项目概览

本项目是一个 Windows 桌面应用程序，核心功能是**为视频自动生成中文字幕**。

### 1.1 核心流程
1. **提取音频**: 将视频文件中的声音提取出来 (使用 FFmpeg)。
2. **语音识别**: 将提取的音频转换成文字字幕 SRT 文件 (使用 Python + Vosk/Whisper)。
3. **字幕合成**: 将 SRT 字幕“烧录”进视频画面中 (使用 FFmpeg)。

### 1.2 技术栈
- **Qt 6 (C++)**: 用于编写图形用户界面 (GUI)，如按钮、列表、进度条等。
- **FFmpeg**: 一个强大的命令行视频处理工具，负责音视频的转换和合成。
- **Python**: 用于调用语音识别库 (Vosk 或 Faster-Whisper)，处理复杂的 AI 任务。

---

## 2. 详细实现原理

### 2.1 C++ 与 Python/FFmpeg 的交互 (QProcess)
Qt 程序本身并不包含视频处理或语音识别的代码，它像一个**指挥官**。
- 当你点击“开始”时，C++ 代码会启动一个外部进程 (cmd 窗口)。
- 比如提取音频时，C++ 会执行命令：`ffmpeg -i input.mp4 ... output.wav`。
- 语音识别时，C++ 会执行：`python scripts/transcribe.py input.wav output.srt`。

**关键代码 (MainWindow.cpp):**
```cpp
// 创建一个进程对象
QProcess *process = new QProcess(this);
// 连接信号：当进程有输出时，读取并显示进度
connect(process, &QProcess::readyReadStandardOutput, [this, process]() {
    QString output = process->readAllStandardOutput();
    // 解析输出中的 "TRANS_PROGRESS: 50" 来更新进度条
});
// 启动命令
process->start("python", arguments);
```

### 2.2 界面设计 (Qt Widgets)
界面主要由以下部分组成：
- **拖拽区 (FileDropListWidget)**: 继承自 `QListWidget`，重写了 `dragEnterEvent` 和 `dropEvent` 函数，实现文件拖入功能。
- **配置区**: 使用 `QComboBox` (下拉框) 选择引擎和模型。
- **进度条**: 接收后台任务的反馈并动态变化。

### 2.3 Python 脚本 (transcribe.py)
这是项目的**大脑**。它接收 WAV 音频，输出 SRT 字幕。
- **Vosk 引擎**: 
  - 这是一个离线识别库。
  - 脚本会读取音频的每一帧，送入 `KaldiRecognizer`。
  - 识别结果包含每个词的时间戳 (start, end, word)。
  - 我们编写了算法 (`split_and_write_srt`) 将长句拆分成短句，避免字幕太长遮挡画面。
- **Whisper 引擎**:
  - 这是一个基于 Transformer 的现代 AI 模型，精度极高。
  - 使用 `faster-whisper` 库，它支持 GPU 加速 (CUDA)。
  - 如果检测到显卡不可用，会自动回退到 CPU 模式。

---

## 3. 如何从零开始实现

如果你想自己动手写一个，可以按照以下步骤：

### 第一步：环境准备
1. **安装 Qt Creator**: 用于编写 C++ 界面。
2. **安装 Python**: 确保 `python` 命令能在终端运行。
3. **下载 FFmpeg**: 将其 `bin` 目录加入环境变量 PATH。

### 第二步：编写 Python 脚本 (验证核心功能)
先不要管界面，写一个 `test.py`：
1. 导入 `vosk` 库。
2. 读取一个 WAV 文件。
3. 打印出识别的文字。
4. 尝试生成 SRT 格式的文本 (格式为: 序号 -> 时间轴 -> 内容)。
*目标：运行脚本，能把一个录音变成 SRT 文件。*

### 第三步：编写 Qt 界面 (C++)
1. 新建一个 `Qt Widgets Application` 项目。
2. 在 `MainWindow.ui` (或者用代码) 拖入一个按钮和一个文本框。
3. 给按钮写点击事件 (`connect` 信号与槽)。
4. 在槽函数中，使用 `QProcess` 调用你刚才写的 `test.py`。

### 第四步：串联 FFmpeg
1. 在 Python 处理前，先用 `QProcess` 调用 `ffmpeg` 把视频转为音频。
2. 在 Python 处理后，再用 `QProcess` 调用 `ffmpeg` 把 SRT 合成进视频。

### 第五步：优化体验
1. **进度条**: 让 Python 脚本打印 `PROGRESS: 10%`，Qt 读取并更新 UI。
2. **拖拽支持**: 搜索 "Qt QListWidget drag drop" 学习如何重写事件。
3. **多线程/异步**: 确保耗时任务不会卡死界面 (QProcess 已经是异步的，很好！)。

---

## 4. 常见问题 (FAQ)

**Q: 为什么我的 Python 脚本在 cmd 运行正常，Qt 里没反应？**
A: 可能是路径问题。Qt 运行时的“当前目录”可能不是脚本所在目录。建议使用绝对路径传递文件参数。

**Q: 为什么进度条不走？**
A: Python 的输出有缓冲。需要在 `print` 后调用 `sys.stdout.flush()`，或者设置环境变量 `PYTHONUNBUFFERED=1`。

**Q: 为什么 Whisper 报错？**
A: Whisper 需要较大的内存和显存。如果报错，尝试换用 `tiny` 模型或检查显卡驱动。

---

希望这份指南能帮助你理解本项目的运作方式！编程最重要的是**拆解问题**：把大功能拆成小步骤，逐个击破。
