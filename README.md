# 视频自动字幕生成器 (Video Subtitle Generator)

这是一个基于 Qt 6.9 (C++)、FFmpeg 和 Python (Vosk / Whisper) 的 Windows 桌面应用程序。它可以自动识别视频中的中文语音，生成 SRT 字幕，并将其“烧录”进视频中（硬字幕），方便制作带字幕的视频内容。

## 🚀 功能特性
- **双引擎支持**: 
  - **Vosk**: 轻量级离线识别，CPU 运行，无需显卡，适合老旧设备。
  - **Whisper (Faster-Whisper)**: 高精度识别，支持 GPU 加速 (CUDA)，准确率远超 Vosk。
- **GPU 加速**: 自动检测并配置 NVIDIA 环境，无需手动安装 CUDA Toolkit（通过 pip 依赖自动注入）。
- **拖拽导入**: 直接将视频文件拖入界面即可添加任务。
- **批量处理**: 自动队列管理，支持多任务顺序处理。
- **硬字幕合成**: 使用 FFmpeg 将字幕直接嵌入视频画面，确保在任何播放器中均可显示。
- **智能排版**: 自动检测长句，将字幕拆分为每行不超过 20 个字符的短句。
- **可视化进度**: 实时显示模型下载、音频转写、**音频提取**和**视频合成**的详细进度 (步骤 1-3)。
- **结果反馈**: 清晰的成功/失败状态指示，失败任务高亮显示。

## 🛠️ 环境搭建指南

在从零开始编译本项目之前，请确保您的 Windows 系统已安装以下环境。

### 1. 基础环境
- **操作系统**: Windows 10 或 Windows 11 (64位)
- **开发工具**: Visual Studio 2022 (需安装 "使用 C++ 的桌面开发" 工作负载)

### 2. 依赖软件安装

#### 2.1 Qt 6.9
1. 下载并运行 Qt Online Installer。
2. 在安装向导中选择 **Qt 6.9.3** 版本。
3. 勾选 **MSVC 2022 64-bit** 组件。
4. (推荐) 勾选 **Qt Shader Tools** 和 **CMake** (位于 Tools 节点下)。
   * *本项目当前配置的 Qt 路径为 `D:/download/qt/6.9.3/msvc2022_64`，如果安装位置不同，需修改配置（见下文）。*

#### 2.2 FFmpeg
1. 下载 FFmpeg Windows 编译版 (推荐 release-full build)。
2. 解压并将 `bin` 目录的路径添加到系统的 **PATH** 环境变量中。
3. **验证**: 打开 PowerShell 输入 `ffmpeg -version`，应显示版本信息。

#### 2.3 Python 环境
1. 安装 Python 3.8 或更高版本 (安装时请勾选 "Add Python to PATH")。
2. 安装项目所需的 Python 库：
   在项目根目录下打开终端，运行：
   ```powershell
   # 使用阿里云镜像源加速下载，并自动安装 NVIDIA GPU 依赖
   pip install -r scripts/requirements.txt -i https://mirrors.aliyun.com/pypi/simple/
   ```
   *(依赖包含: `faster-whisper`, `vosk`, `requests`, `tqdm`, `nvidia-cublas-cu12`, `nvidia-cudnn-cu12`)*

## ⚙️ 编译步骤

### 1. 获取代码
确保已下载本项目代码，目录结构应如下：
```
wavToTxt/
├── CMakeLists.txt
├── src/           (C++ 源码)
├── scripts/       (Python 脚本)
└── docs/          (文档)
```

### 2. 配置 CMake
打开项目根目录下的 `CMakeLists.txt`，检查第 12 行：
```cmake
set(CMAKE_PREFIX_PATH "D:/download/qt/6.9.3/msvc2022_64")
```
⚠️ **重要**: 如果您的 Qt 安装目录不是 `D:/download/qt/6.9.3/msvc2022_64`，请务必将其修改为您实际的 Qt MSVC 路径。

### 3. 执行编译

#### 方法 A: 一键编译脚本 (推荐)
项目根目录下提供了一键编译脚本 `build_project.bat`，它会自动配置环境并编译 Release 版本。
直接双击运行或在终端执行：
```powershell
.\build_project.bat
```

#### 方法 B: 手动编译
在项目根目录下打开 PowerShell，执行以下命令：

```powershell
# 1. 创建构建目录并生成 Visual Studio 工程文件
# 如果您的 cmake 没有添加到 PATH，请使用 Qt 工具目录下的 cmake 全路径
cmake -B build -G "Visual Studio 17 2022" -A x64

# 2. 编译 Release 版本
cmake --build build --config Release
```

如果编译成功，您将在终端看到 `Build files have been written to...` 和 `0 Error(s)`。

## ▶️ 运行程序

### 方法 A: 通过 Qt 部署工具 (推荐)
为了在没有 Qt 环境的电脑上运行，或者避免缺少 DLL 的问题，建议运行部署工具。

```powershell
# 请替换为您的 windeployqt 实际路径
D:\download\qt\6.9.3\msvc2022_64\bin\windeployqt.exe build\Release\VideoSubtitleGenerator.exe
```
执行后，双击 `build\Release\VideoSubtitleGenerator.exe` 即可运行。

### 方法 B: 直接运行
如果您的系统 PATH 中已经包含了 Qt 的 bin 目录 (`.../msvc2022_64/bin`)，则可以直接运行生成的可执行文件：
- `build/Release/VideoSubtitleGenerator.exe`

## 📖 使用说明

1. **启动程序**: 打开 `VideoSubtitleGenerator.exe`。
2. **添加视频**: 
   - 将视频文件拖拽到左侧列表区域。
   - 或点击左上角“添加视频”按钮选择文件。
3. **设置输出 (可选)**:
   - 默认输出在原视频同级目录。
   - 点击“选择目录”可自定义所有任务的输出位置。
4. **配置模型**:
   - **Vosk**: 默认引擎，适合纯离线、低配置环境。
   - **Whisper**: 推荐引擎，精度高但需要 NVIDIA 显卡支持。
     - **Tiny / Base**: 速度极快，显存需求低 (~1GB)，适合快速预览。
     - **Small**: **推荐**，平衡速度与精度 (需 ~2GB 显存)，适合大多数 2G 显存用户。
     - **Medium**: 高精度，适合复杂音频，显存需求较高 (~5GB)。
     - **Large**: 最高精度，显存需求极高 (~10GB)。
5. **开始处理**: 
   - 程序会自动开始处理队列中的任务。
   - 界面底部会显示当前步骤的进度 (提取音频 -> 转录 -> 合成)。
   - **首次运行 Whisper 时会自动下载模型**，请保持网络连接（已配置国内镜像加速）。
6. **查看结果**:
   - 处理成功的视频会显示在右侧列表，名为 `原文件名_subtitled.mp4`。
   - 失败的任务会显示为红色，并注明失败阶段（如音频提取失败、转写错误等）。

## ❓ 常见问题 (FAQ)

### Q1: 为什么 Whisper 转录失败，提示 `cudnn_ops64_9.dll not found`？
**A**: 这是因为系统缺少 NVIDIA 运行库。本项目已在 `requirements.txt` 中添加了自动修复依赖。
**解决方法**: 请重新运行 `pip install -r scripts/requirements.txt`，程序会自动注入所需的 DLL，无需手动安装 CUDA Toolkit。

### Q2: Whisper 模型下载很慢？
**A**: 程序已内置 `hf-mirror.com` 镜像加速。如果仍然很慢，请检查网络连接。首次下载后模型会缓存，下次运行无需下载。

### Q3: 为什么转录出来的字幕是英文？
**A**: 之前的版本可能会自动检测为英文。**最新版本已强制指定语言为中文 (zh)**，请确保使用的是最新的 `transcribe.py` 脚本。

### Q4: 2G 显存应该选哪个模型？
**A**: 推荐选择 **Small** 模型。它使用 INT8 量化，显存占用约 2GB，正好适合您的配置。如果仍然爆显存，请尝试 **Base** 模型。

### Q5: 为什么生成的 SRT 字幕文件为空？
**A**: 可能原因包括：
1. **音频静音**: 视频中没有检测到语音。
2. **模型检测问题**: 尝试更换其他模型 (如从 Small 换成 Medium 或 Vosk) 再试。
3. **VAD 过滤**: 某些情况下静音过滤过于激进。最新版脚本已优化此逻辑。
程序会输出警告日志，请留意界面下方的日志窗口。

### Q6: 提示“无法复制字幕文件到临时路径”或“合成视频失败”？
**A**: 这通常是因为上一步生成的 SRT 文件为空或丢失。
- 请检查输出目录下生成的 `.srt` 文件内容。
- 确保 Python 脚本运行正常（无报错退出）。
- **注意**: 程序在合成视频时会生成名为 `temp_render_subs.srt` 的临时文件，合成完成后会自动删除。如果合成失败，可能是因为该文件未成功生成。

### Q7: 为什么 Whisper 运行时 GPU 显存占用正常但没有输出字幕？
**A**: 这通常与 GPU 浮点精度有关。
- 程序已针对 GTX 1050 / GTX 1650 等旧款显卡进行了优化，强制使用 `int8_float32` 混合精度计算，避免了 FP16 在某些显卡上输出乱码或崩溃的问题。
- 如果仍有问题，请尝试更新显卡驱动。

## 📚 更多文档
- [接口文档 (INTERFACE.md)](docs/INTERFACE.md): 详细说明了 C++ 与 Python/FFmpeg 的交互协议。
- [项目进度 (PROGRESS.md)](docs/PROGRESS.md): 记录项目开发日志和待办事项。

## 📂 目录结构说明
- `src/`: C++ 源代码 (MainWindow, FileDropListWidget)。
- `scripts/`: Python 转录脚本 (`transcribe.py`)。
- `build/`: 编译生成目录 (自动生成)。
- `docs/`: 项目文档。
