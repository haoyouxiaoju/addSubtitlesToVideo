#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QProcess>
#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>
#include <QApplication>
#include <QDir>
#include <QGroupBox>
#include <QSplitter>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QMenu>
#include <QScrollBar>

/**
 * @brief 构造函数，初始化UI
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), isProcessing(false), currentStage(StageNone), totalDurationSecs(0), currentProcess(nullptr)
{
    initUI();
}

MainWindow::~MainWindow()
{
    // 析构时确保进程已清理
    if (currentProcess) {
        if (currentProcess->state() != QProcess::NotRunning) {
            currentProcess->kill();
            currentProcess->waitForFinished(1000);
        }
    }
}

/**
 * @brief 窗口关闭事件
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (isProcessing && currentProcess) {
        if (currentProcess->state() != QProcess::NotRunning) {
            // 提示用户还是直接杀掉？为了防止显存残留，直接强杀比较安全
            // 或者可以弹窗提示，但用户既然点了关闭，通常期望程序退出
            log("正在终止后台进程...");
            currentProcess->kill(); // 强制杀死
            currentProcess->waitForFinished(2000); // 等待最多2秒
        }
    }
    event->accept();
}

/**
 * @brief 初始化UI
 */
void MainWindow::initUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    setWindowTitle("视频自动字幕生成器");
    resize(900, 700);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // 1. 顶部配置区
    QGroupBox *configGroup = new QGroupBox("配置");
    QHBoxLayout *topLayout = new QHBoxLayout(configGroup);
    
    addFilesButton = new QPushButton("添加视频");
    // addFilesButton->setIcon(QIcon::fromTheme("list-add"));
    connect(addFilesButton, &QPushButton::clicked, this, &MainWindow::addVideoFiles);

    // 引擎选择
    QLabel *engineLabel = new QLabel("引擎:");
    engineCombo = new QComboBox();
    engineCombo->addItem("Vosk (离线/CPU)", "vosk");
    engineCombo->addItem("Whisper (GPU/精度高)", "whisper");

    // 模型选择
    QLabel *modelLabel = new QLabel("模型:");
    modelCombo = new QComboBox();
    modelCombo->addItem("Tiny (极快/1G显存)", "tiny");
    modelCombo->addItem("Base (快/1G显存)", "base");
    modelCombo->addItem("Small (推荐/2G显存)", "small");
    modelCombo->addItem("Medium (精准/5G显存)", "medium");
    modelCombo->addItem("Large (最准/10G显存)", "large");
    modelCombo->setCurrentIndex(2); // Default to Small for 2G GPU

    // 帮助按钮
    helpButton = new QPushButton("模型指南");
    connect(helpButton, &QPushButton::clicked, [this]() {
        QMessageBox::information(this, "Whisper模型选择指南", 
            "Whisper模型显存需求参考 (int8量化):\n\n"
            "Tiny:   ~1 GB 显存 (极快，精度较低)\n"
            "Base:   ~1 GB 显存 (快，精度一般)\n"
            "Small:  ~2 GB 显存 (推荐2G显卡，平衡)\n"
            "Medium: ~5 GB 显存 (高精度，速度较慢)\n"
            "Large:  ~10 GB 显存 (最高精度，速度慢)\n\n"
            "注意: 如果显存不足，程序将自动回退到CPU运行，速度会显著变慢。");
    });

    // 联动逻辑
    connect(engineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        bool isWhisper = (engineCombo->itemData(index).toString() == "whisper");
        modelCombo->setEnabled(isWhisper);
        helpButton->setEnabled(isWhisper);
    });
    // 初始化状态
    modelCombo->setEnabled(false); // Default is Vosk
    helpButton->setEnabled(false);
    
    outputDirEdit = new QLineEdit();
    outputDirEdit->setPlaceholderText("输出目录 (默认保存在原视频目录)");
    outputDirEdit->setReadOnly(true);
    
    selectOutputDirButton = new QPushButton("选择目录");
    // selectOutputDirButton->setIcon(QIcon::fromTheme("folder-open"));
    connect(selectOutputDirButton, &QPushButton::clicked, this, &MainWindow::selectOutputDir);

    topLayout->addWidget(addFilesButton);
    topLayout->addWidget(new QLabel("|"));
    topLayout->addWidget(engineLabel);
    topLayout->addWidget(engineCombo);
    topLayout->addWidget(modelLabel);
    topLayout->addWidget(modelCombo);
    topLayout->addWidget(helpButton);
    topLayout->addWidget(new QLabel("|"));
    topLayout->addWidget(outputDirEdit);
    topLayout->addWidget(selectOutputDirButton);
    mainLayout->addWidget(configGroup);

    // 2. 中间功能区 (功能列表 + 任务列表)
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    
    // 左侧：待处理列表 (支持拖拽)
    QGroupBox *inputGroup = new QGroupBox("待处理队列 (支持拖拽视频文件到此处)");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputListWidget = new FileDropListWidget();
    inputListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection); // 支持多选
    // 右键菜单
    inputListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(inputListWidget, &QWidget::customContextMenuRequested, [this](const QPoint &pos) {
        QMenu menu(this);
        QAction *delAction = menu.addAction("从队列移除");
        connect(delAction, &QAction::triggered, this, &MainWindow::removeSelectedTask);
        menu.exec(inputListWidget->mapToGlobal(pos));
    });
    // 连接拖拽信号
    connect(inputListWidget, &FileDropListWidget::filesDropped, this, &MainWindow::handleDroppedFiles);
    
    inputLayout->addWidget(inputListWidget);
    splitter->addWidget(inputGroup);

    // 右侧：已完成列表 / 日志
    QGroupBox *outputGroup = new QGroupBox("处理结果");
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);
    outputListWidget = new QListWidget();
    outputLayout->addWidget(outputListWidget);
    splitter->addWidget(outputGroup);

    // 设置分割比例
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter, 1); // 占据主要空间

    // 3. 底部状态区
    QGroupBox *statusGroup = new QGroupBox("状态与进度");
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
    
    // 功能说明标签
    QLabel *infoLabel = new QLabel("当前功能: 1. 音频提取(FFmpeg) -> 2. 语音转录(Whisper/Vosk) -> 3. 字幕合成(FFmpeg)");
    infoLabel->setStyleSheet("color: #666; font-style: italic;");
    statusLayout->addWidget(infoLabel);

    // 进度条
    progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    statusLayout->addWidget(progressBar);

    // 状态文本
    statusLabel = new QLabel("就绪");
    statusLayout->addWidget(statusLabel);
    
    mainLayout->addWidget(statusGroup);

    // 日志区域
    logArea = new QTextEdit();
    logArea->setReadOnly(true);
    logArea->setMaximumHeight(100);
    logArea->setPlaceholderText("运行日志将显示在这里...");
    statusLayout->addWidget(logArea);
}

/**
 * @brief 添加视频文件
 */
void MainWindow::addVideoFiles()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this, "选择视频文件", "", "视频文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv)");
    addVideosToQueue(fileNames);
}

/**
 * @brief 处理拖拽添加的文件
 */
void MainWindow::handleDroppedFiles(const QStringList &files)
{
    addVideosToQueue(files);
}

/**
 * @brief 添加视频到队列的内部逻辑
 */
void MainWindow::addVideosToQueue(const QStringList &files)
{
    if (files.isEmpty()) return;

    int addedCount = 0;
    for (const QString &fileName : files) {
        // 检查是否已存在于队列中 (简单去重)
        bool exists = false;
        for (const auto &task : taskList) {
            if (task.inputPath == fileName) {
                exists = true;
                break;
            }
        }
        if (exists) continue;

        TaskInfo task;
        task.inputPath = fileName;
        task.outputDir = outputDirEdit->text();
        task.status = "Pending";
        
        taskList.append(task);
        inputListWidget->addItem(fileName);
        log("已添加任务: " + fileName);
        addedCount++;
    }
    
    if (addedCount > 0) {
        statusLabel->setText(QString("队列中: %1 个任务").arg(taskList.size()));
        // 如果当前没有在处理，自动开始
        if (!isProcessing) {
            processNextTask();
        }
    }
}

/**
 * @brief 从队列中删除选中项
 */
void MainWindow::removeSelectedTask()
{
    QList<QListWidgetItem*> items = inputListWidget->selectedItems();
    if (items.isEmpty()) return;

    for (auto item : items) {
        QString path = item->text();
        // 从任务列表中移除
        for (int i = 0; i < taskList.size(); ++i) {
            if (taskList[i].inputPath == path) {
                // 如果正在处理该任务，则不移除 (或者需要停止逻辑，这里简单处理: 正在处理的不移除)
                if (isProcessing && i == 0) { // 假设第一个是正在处理的
                     QMessageBox::warning(this, "无法移除", "该任务正在处理中，无法移除: " + QFileInfo(path).fileName());
                     continue;
                }
                taskList.removeAt(i);
                delete inputListWidget->takeItem(inputListWidget->row(item));
                log("已移除任务: " + path);
                break;
            }
        }
    }
    statusLabel->setText(QString("队列中: %1 个任务").arg(taskList.size()));
}

/**
 * @brief 选择输出目录
 */
void MainWindow::selectOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择输出目录");
    if (!dir.isEmpty()) {
        outputDirEdit->setText(dir);
        log("输出目录设置为: " + dir);
        // 更新队列中尚未开始的任务的输出目录
        for (int i = 0; i < taskList.size(); ++i) {
             // 如果正在处理 (i==0 && isProcessing)，则不修改当前正在跑的任务
             if (isProcessing && i == 0) continue;
             taskList[i].outputDir = dir;
        }
    }
}

/**
 * @brief 记录日志
 */
void MainWindow::log(const QString &message)
{
    logArea->append(message);
    // 滚动到底部
    logArea->verticalScrollBar()->setValue(logArea->verticalScrollBar()->maximum());
}

/**
 * @brief 处理下一个任务
 */
void MainWindow::processNextTask()
{
    if (taskList.isEmpty()) {
        isProcessing = false;
        statusLabel->setText("所有任务完成");
        QMessageBox::information(this, "完成", "所有视频处理完成!");
        progressBar->setValue(100);
        return;
    }

    isProcessing = true;
    currentTask = taskList.first(); // 获取第一个但不移除，处理完再移除
    
    // 高亮当前正在处理的项
    QList<QListWidgetItem*> items = inputListWidget->findItems(currentTask.inputPath, Qt::MatchExactly);
    if (!items.isEmpty()) {
        items.first()->setBackground(QColor("#e6f7ff")); // 浅蓝色背景
        items.first()->setText(currentTask.inputPath + " (处理中...)");
    }

    log("==========================================");
    log("开始处理: " + currentTask.inputPath);
    progressBar->setValue(0);
    
    // 重置状态
    currentStage = StageExtract;
    totalDurationSecs = 0;

    // 准备路径
    QFileInfo fileInfo(currentTask.inputPath);
    QString baseName = fileInfo.completeBaseName();
    QString sourceDir = fileInfo.absolutePath();
    
    // 确定输出目录
    QString targetDir = currentTask.outputDir;
    if (targetDir.isEmpty()) {
        targetDir = sourceDir;
    }

    // 使用临时目录存放中间文件 (与输入文件同目录，避免跨盘问题)
    tempAudioPath = sourceDir + "/" + baseName + "_temp_audio.wav";
    outputSubtitlePath = targetDir + "/" + baseName + ".srt";
    
    // 如果输出目录与源目录不同，确保输出目录存在
    QDir dir(targetDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    currentTask.outputVideoPath = targetDir + "/" + baseName + "_subtitled." + fileInfo.suffix();

    // 检查并删除旧文件
    if (QFile::exists(tempAudioPath)) QFile::remove(tempAudioPath);
    if (QFile::exists(outputSubtitlePath)) QFile::remove(outputSubtitlePath);
    if (QFile::exists(currentTask.outputVideoPath)) QFile::remove(currentTask.outputVideoPath);

    log("正在提取音频...");
    statusLabel->setText("步骤 1/3: 提取音频 - " + baseName);
    progressBar->setValue(5);

    // ffmpeg -i input.mp4 -ac 1 -ar 16000 -f wav temp_audio.wav
    // 使用 nativeSeparators 确保路径分隔符正确 (虽然 Qt 通常能处理，但 FFmpeg 有时对中文路径敏感)
    QString nativeInputPath = QDir::toNativeSeparators(currentTask.inputPath);
    QString nativeTempAudioPath = QDir::toNativeSeparators(tempAudioPath);

    QStringList args;
    args << "-y" << "-i" << nativeInputPath << "-ac" << "1" << "-ar" << "16000" << "-f" << "wav" << nativeTempAudioPath;
    runCommand("ffmpeg", args);
}

/**
 * @brief 运行外部命令 (复用或创建进程)
 */
void MainWindow::runCommand(const QString &program, const QStringList &arguments, const QString &workDir)
{
    // 如果没有当前进程，或者当前进程正在被销毁，则创建新进程
    if (!currentProcess) {
        currentProcess = new QProcess(this);
        connect(currentProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onProcessFinished(int, QProcess::ExitStatus)));
        connect(currentProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessReadyReadStandardOutput);
        connect(currentProcess, &QProcess::readyReadStandardError, this, &MainWindow::onProcessReadyReadStandardError);
    }
    
    // 确保进程处于非运行状态
    if (currentProcess->state() != QProcess::NotRunning) {
        currentProcess->kill();
        currentProcess->waitForFinished();
    }

    if (!workDir.isEmpty()) {
        currentProcess->setWorkingDirectory(workDir);
    }
    
    // 设置进程环境，强制 Python 不缓冲输出
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUNBUFFERED", "1");
    env.insert("PYTHONUTF8", "1"); // 强制 Python 使用 UTF-8 输出
    currentProcess->setProcessEnvironment(env);
    
    log("执行命令: " + program + " " + arguments.join(" "));
    currentProcess->start(program, arguments);
    
    if (!currentProcess->waitForStarted()) {
        log("错误: 无法启动程序 " + program);
        // 如果启动失败，手动触发失败回调 (Exit Code -1)
        onProcessFinished(-1, QProcess::NormalExit);
    }
}

/**
 * @brief 统一处理进程完成信号
 */
void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 根据当前阶段分发处理逻辑
    if (currentStage == StageExtract) {
        onExtractAudioFinished(exitCode);
    } else if (currentStage == StageTranscribe) {
        onTranscribeFinished(exitCode);
    } else if (currentStage == StageEmbed) {
        onEmbedSubtitleFinished(exitCode);
    } else {
        log("未知阶段的任务完成: " + QString::number(exitCode));
    }
    
    // 注意：不再调用 deleteLater，因为我们要复用 currentProcess
    // 只有在 closeEvent 时才销毁
}

/**
 * @brief 统一处理标准输出
 */
void MainWindow::onProcessReadyReadStandardOutput()
{
    QProcess *process = qobject_cast<QProcess*>(sender());
    if (!process) return;

    // 循环读取所有可用行，确保不遗漏
    while(process->canReadLine()) {
        QString line = QString::fromUtf8(process->readLine()).trimmed();
        if (line.isEmpty()) continue;
        
        // 检查下载进度: DOWNLOAD_PROGRESS: 45
        if (line.contains("DOWNLOAD_PROGRESS:")) {
            int idx = line.lastIndexOf("DOWNLOAD_PROGRESS:");
            QString valStr = line.mid(idx + 18).trimmed();
            bool ok;
            int percent = valStr.toInt(&ok);
            if (ok) {
                statusLabel->setText(QString("正在下载模型: %1%").arg(percent));
            }
        }
        // 检查转录进度: TRANS_PROGRESS: 50
        else if (line.contains("TRANS_PROGRESS:")) {
            int idx = line.lastIndexOf("TRANS_PROGRESS:");
            QString valStr = line.mid(idx + 15).trimmed();
            bool ok;
            int percent = valStr.toInt(&ok);
            if (ok) {
                // 映射到总进度 30-80
                int totalPercent = 30 + (percent / 2); 
                progressBar->setValue(totalPercent);
                statusLabel->setText(QString("正在转录: %1%").arg(percent));
            }
        }
        // 其他重要信息直接显示
        else {
            // 只有当不是进度信息时才打印到日志，避免日志刷屏
            log("Python: " + line);
        }
    }
}

/**
 * @brief 统一处理标准错误
 */
void MainWindow::onProcessReadyReadStandardError()
{
    QProcess *process = qobject_cast<QProcess*>(sender());
    if (!process) return;

    QByteArray data = process->readAllStandardError();
    QString output = QString::fromUtf8(data);
    
    // 分割行，处理可能混合在一起的 \r (FFmpeg进度) 和 \n (普通日志)
    // 简单替换 \r 为 \n 然后分割
    QString cleanOutput = output;
    cleanOutput.replace('\r', '\n');
    QStringList lines = cleanOutput.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;
        
        // FFmpeg 进度解析
        // 1. 获取总时长: Duration: 00:00:10.50
        if ((currentStage == StageExtract || currentStage == StageEmbed) && trimmedLine.contains("Duration: ") && totalDurationSecs <= 0.1) {
            int idx = trimmedLine.indexOf("Duration: ");
            // 提取 00:00:00.00 格式
            QString timeStr = trimmedLine.mid(idx + 10, 11); 
            QStringList parts = timeStr.split(":");
            if (parts.size() == 3) {
                double h = parts[0].toDouble();
                double m = parts[1].toDouble();
                double s = parts[2].toDouble();
                totalDurationSecs = h * 3600 + m * 60 + s;
            }
        }
        
        // 2. 获取当前进度: time=00:00:05.20
        if ((currentStage == StageExtract || currentStage == StageEmbed) && trimmedLine.contains("time=") && totalDurationSecs > 0) {
                int idx = trimmedLine.indexOf("time=");
                // 找到空格作为结束，或者行尾
                int endIdx = trimmedLine.indexOf(" ", idx);
                if (endIdx == -1) endIdx = trimmedLine.length();
                QString timeStr = trimmedLine.mid(idx + 5, endIdx - (idx + 5));
                
                QStringList parts = timeStr.split(":");
                if (parts.size() == 3) {
                    double h = parts[0].toDouble();
                    double m = parts[1].toDouble();
                    double s = parts[2].toDouble();
                    double currentSecs = h * 3600 + m * 60 + s;
                    
                    int percent = (int)((currentSecs / totalDurationSecs) * 100);
                    if (percent > 100) percent = 100;
                    if (percent < 0) percent = 0;
                    
                    // 根据阶段更新进度条
                    if (currentStage == StageExtract) {
                        // 0-30%
                        int val = (int)(percent * 0.3);
                        if (val > 30) val = 30;
                        progressBar->setValue(val);
                        statusLabel->setText(QString("步骤 1/3: 提取音频 - %1%").arg(percent));
                    } else if (currentStage == StageEmbed) {
                        // 80-100%
                        int val = 80 + (int)(percent * 0.2);
                        if (val > 100) val = 100;
                        progressBar->setValue(val);
                        statusLabel->setText(QString("步骤 3/3: 合成字幕 - %1%").arg(percent));
                    }
                }
        }

        // 忽略 tqdm 的某些非关键输出，避免干扰状态栏
        if (trimmedLine.contains("|") && trimmedLine.contains("/") && trimmedLine.contains("[")) {
                continue; 
        }

        // 记录错误或警告信息
        if (trimmedLine.contains("Error", Qt::CaseInsensitive) || 
            trimmedLine.contains("Warning", Qt::CaseInsensitive) ||
            trimmedLine.contains("Exception", Qt::CaseInsensitive) ||
            trimmedLine.contains("Traceback", Qt::CaseInsensitive) ||
            trimmedLine.contains("download", Qt::CaseInsensitive)) {
            log("STDERR: " + trimmedLine);
        }
    }
}

/**
 * @brief 音频提取完成回调
 */
void MainWindow::onExtractAudioFinished(int exitCode)
{
    // 不再这里销毁 process
    
    if (exitCode != 0) {
        log("错误: 音频提取失败");
        // 标记失败
        QListWidgetItem *item = new QListWidgetItem(currentTask.inputPath + " -> 失败 (音频提取)");
        item->setForeground(Qt::red); // 红色字体
        outputListWidget->addItem(item);
        
        // 从输入列表移除
        if (!taskList.isEmpty()) taskList.removeFirst();
        QList<QListWidgetItem*> items = inputListWidget->findItems(currentTask.inputPath + " (处理中...)", Qt::MatchExactly);
        if (!items.isEmpty()) delete inputListWidget->takeItem(inputListWidget->row(items.first()));
        
        processNextTask();
        return;
    }

    log("音频提取完成，开始转录...");
    statusLabel->setText("步骤 2/3: 语音转写 - " + QFileInfo(currentTask.inputPath).baseName());
    progressBar->setValue(30);
    
    currentStage = StageTranscribe;

    // python transcribe.py input.wav output.srt
    // 获取当前可执行文件目录的上级目录中的 scripts/transcribe.py
    QString appDir = QCoreApplication::applicationDirPath();
    // 假设结构是 build/Debug/VideoSubtitleGenerator.exe -> scripts 在 build/../scripts
    // 或者直接在 src/../scripts
    // 我们多试几个路径
    QString scriptPath = appDir + "/../scripts/transcribe.py";
    if (!QFile::exists(scriptPath)) {
        scriptPath = appDir + "/scripts/transcribe.py";
    }
    if (!QFile::exists(scriptPath)) {
        // 尝试源码目录 (假设 d:/myfiles/code/wavToTxt)
        scriptPath = "d:/myfiles/code/wavToTxt/scripts/transcribe.py";
    }
    
    QString engine = engineCombo->currentData().toString();
    QString model = modelCombo->currentData().toString();

    QStringList args;
    args << scriptPath << tempAudioPath << outputSubtitlePath << "--engine" << engine << "--model" << model;
    runCommand("python", args);
}

/**
 * @brief 转录完成回调
 */
void MainWindow::onTranscribeFinished(int exitCode)
{
    // 不再销毁进程，以便复用
    QString errorOutput = currentProcess ? QString(currentProcess->readAllStandardError()) : QString("");
    if (!errorOutput.isEmpty()) {
        log("Python 错误输出: " + errorOutput);
    }

    if (exitCode != 0) {
        log("错误: 语音转写失败");
        QListWidgetItem *item = new QListWidgetItem(currentTask.inputPath + " -> 失败 (转写错误)");
        item->setForeground(Qt::red);
        outputListWidget->addItem(item);

        if (!taskList.isEmpty()) taskList.removeFirst();
        QList<QListWidgetItem*> items = inputListWidget->findItems(currentTask.inputPath + " (处理中...)", Qt::MatchExactly);
        if (!items.isEmpty()) delete inputListWidget->takeItem(inputListWidget->row(items.first()));

        processNextTask();
        return;
    }

    // 准备硬字幕合成
    QString targetDir = QFileInfo(currentTask.outputVideoPath).absolutePath();
    QString tempSrtName = "temp_render_subs.srt";
    QString tempSrtPath = targetDir + "/" + tempSrtName;

    // 检查字幕文件是否存在且不为空
    QFileInfo srtInfo(outputSubtitlePath);
    if (!srtInfo.exists() || srtInfo.size() == 0) {
        // 如果文件不存在或为空，可能是Python脚本虽然exit(0)但没有生成有效内容
        // 或者确实是静音文件
        log("错误: 字幕文件无效 (未检测到语音或生成失败): " + outputSubtitlePath);
        
        QListWidgetItem *item = new QListWidgetItem(currentTask.inputPath + " -> 失败 (字幕无效)");
        item->setForeground(Qt::red);
        outputListWidget->addItem(item);

        if (!taskList.isEmpty()) taskList.removeFirst();
        QList<QListWidgetItem*> items = inputListWidget->findItems(currentTask.inputPath + " (处理中...)", Qt::MatchExactly);
        if (!items.isEmpty()) delete inputListWidget->takeItem(inputListWidget->row(items.first()));

        processNextTask();
        return;
    }

    // 复制 SRT
    if (QFile::exists(tempSrtPath)) QFile::remove(tempSrtPath);
    QFile::copy(outputSubtitlePath, tempSrtPath);

    log("语音转写完成，开始合成视频(硬字幕)...");
    statusLabel->setText("步骤 3/3: 合成字幕(硬字幕) - " + QFileInfo(currentTask.inputPath).baseName());
    progressBar->setValue(80);
    
    currentStage = StageEmbed;
    totalDurationSecs = 0; // 重置，重新从 FFmpeg 输出获取时长

    // ffmpeg -i input.mp4 -vf subtitles='subs.srt' -c:v libx264 -preset fast -c:a copy output.mp4
    // 注意: subtitles 滤镜路径问题比较麻烦，使用相对路径最稳妥
    // 另外 FFmpeg 的 subtitles 滤镜在 Windows 下处理路径非常棘手，尤其是中文路径和盘符冒号
    // 官方建议：
    // 1. 路径分隔符必须是 / 或 \\\\ (转义)
    // 2. 盘符冒号需要转义，例如 D\:/path/to/file
    // 3. 但如果使用相对路径且在同一目录下运行，通常可以避免这些问题。
    // 我们这里采用相对路径 temp_render_subs.srt，并且设置工作目录为 outputVideoPath 所在目录
    
    // 确保路径分隔符正确 (Windows下 ffmpeg 有时偏好 /)
    QString nativeInputPath = QDir::toNativeSeparators(currentTask.inputPath);
    QString nativeOutputVideoPath = QDir::toNativeSeparators(currentTask.outputVideoPath);

    // 对于 subtitles 滤镜中的文件名，FFmpeg 需要特殊的转义
    // 但因为我们已经在 targetDir 下运行，且文件名为简单的 temp_render_subs.srt，不含特殊字符，直接用文件名即可
    
    QStringList args;
    args << "-y" << "-i" << nativeInputPath << "-vf" << QString("subtitles='%1'").arg(tempSrtName) 
         << "-c:v" << "libx264" << "-preset" << "fast" << "-c:a" << "copy" << nativeOutputVideoPath;
    
    // 传递工作目录 targetDir
    runCommand("ffmpeg", args, targetDir);
}

/**
 * @brief 合成完成回调
 */
void MainWindow::onEmbedSubtitleFinished(int exitCode)
{
    // 不再销毁进程，以便复用


    if (exitCode != 0) {
        log("错误: 视频合成失败");
        QListWidgetItem *item = new QListWidgetItem(currentTask.inputPath + " -> 失败 (合成错误)");
        item->setForeground(Qt::red);
        outputListWidget->addItem(item);
    } else {
        log("任务完成! 输出文件: " + currentTask.outputVideoPath);
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(currentTask.inputPath).fileName() + " -> " + currentTask.outputVideoPath);
        item->setForeground(Qt::darkGreen); // 绿色表示成功
        outputListWidget->addItem(item);
        progressBar->setValue(100);
    }

    // 清理临时文件
    if (QFile::exists(tempAudioPath)) {
        QFile::remove(tempAudioPath);
    }
    
    // 清理渲染用的临时字幕文件
    QString targetDir = QFileInfo(currentTask.outputVideoPath).absolutePath();
    QString tempSrtPath = targetDir + "/temp_render_subs.srt";
    if (QFile::exists(tempSrtPath)) {
        QFile::remove(tempSrtPath);
    }

    // 从任务队列和待处理列表移除
    if (!taskList.isEmpty()) taskList.removeFirst();
    QList<QListWidgetItem*> items = inputListWidget->findItems(currentTask.inputPath + " (处理中...)", Qt::MatchExactly);
    if (!items.isEmpty()) delete inputListWidget->takeItem(inputListWidget->row(items.first()));

    // 继续下一个任务
    processNextTask();
}
