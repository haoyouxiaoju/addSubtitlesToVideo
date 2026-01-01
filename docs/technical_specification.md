# 技术规格说明书

## 1. 接口文档

### 1.1 Python 转录脚本接口
**路径**: `scripts/transcribe.py`

**调用方式**:
```bash
python transcribe.py <input_wav> <output_srt>
```

**参数**:
- `input_wav`: 输入的音频文件路径 (WAV格式, 单声道, 16kHz推荐)
- `output_srt`: 输出的字幕文件路径 (SRT格式)

**返回值**:
- 0: 成功
- 1: 失败 (参数错误, 文件不存在, 模型加载失败等)

**输出**:
- 标准输出 (stdout): 进度日志 (包含 `DOWNLOAD_PROGRESS: <percent>` 和 `TRANS_PROGRESS: <percent>` 标记)
- 标准错误 (stderr): 错误信息

### 1.2 C++ 内部接口
**主要类**:
- `MainWindow`: 主窗口逻辑控制
- `FileDropListWidget`: 支持拖拽的文件列表控件

**关键逻辑说明**:
- **字幕合成**: 采用硬字幕 (Hard Subtitle) 方式，使用 FFmpeg 的 `libx264` 编码器和 `subtitles` 滤镜，确保字幕兼容性和显示效果。
- **路径处理**: 为避免 FFmpeg 滤镜路径转义问题，采用将 SRT 复制到输出目录并使用相对路径引用的策略。

**MainWindow 核心槽函数**:
- `addVideoFiles()`: 通过文件对话框添加视频
- `handleDroppedFiles(const QStringList &files)`: 处理拖拽添加的文件
- `removeSelectedTask()`: 从队列中移除选中的任务
- `processNextTask()`: 处理队列中的下一个任务
- `onExtractAudioFinished(int exitCode)`: 音频提取完成回调
- `onTranscribeFinished(int exitCode)`: 转录完成回调
- `onEmbedSubtitleFinished(int exitCode)`: 视频合成完成回调

## 2. 数据库表结构
本项目不涉及数据库存储。

## 3. 配置文件
本项目目前没有外部配置文件。
相关配置 (如模型路径, FFmpeg参数) 硬编码在 `MainWindow.cpp` 和 `transcribe.py` 中。
- 模型名称: `vosk-model-small-cn-0.22`
- 模型下载地址: `https://alphacephei.com/vosk/models/`

## 4. 日志格式
**UI日志**:
显示在主界面的文本框中。
- 常规日志: 黑色文本
- 进度更新: 更新进度条和状态标签

**任务列表状态**:
- 待处理队列 (`FileDropListWidget`): 显示视频文件路径
- 处理结果列表 (`QListWidget`): 
  - 成功: 绿色文本, 格式 "文件名 -> 输出路径"
  - 失败: 红色文本, 格式 "文件名 -> 失败 (原因)"
  - 处理中: 浅蓝色背景高亮

**控制台日志**:
- C++程序输出 Qt debug 信息
- Python脚本输出带有进度标记的日志

## 5. 异常处理逻辑

### 5.1 C++ 层
- **文件选择**: 支持多选，自动去重。
- **任务队列**: 
  - 支持删除未开始的任务。
  - 正在处理的任务无法删除，会有弹窗提示。
- **外部命令执行**:
  - 监听 `QProcess` 输出，实时更新进度。
  - 检查 `exitCode`，如果非0，则视为失败。
  - 失败处理: 
    - 在结果列表中添加红色失败条目。
    - 从待处理队列移除该任务。
    - 自动开始处理队列中的下一个任务。

### 5.2 Python 层
- **参数检查**: 检查命令行参数数量。
- **网络异常**: 模型下载时尝试自动处理代理问题，如果下载失败抛出异常。
- **模型加载**: 加载失败打印错误并退出。

## 6. 依赖项
- **C++**: Qt 6.9 (Widgets, Core, Gui)
- **Runtime**: FFmpeg, Python 3.x
- **Python Libs**: vosk, requests, tqdm (可选)
