#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QListWidget>
#include <QQueue>
#include <QCloseEvent>
#include <QProcess>
#include "FileDropListWidget.h"

/**
 * @brief 任务信息结构体
 */
struct TaskInfo {
    QString inputPath;
    QString outputDir;
    QString status; // "Pending", "Processing", "Completed", "Failed"
    QString outputVideoPath;
};

/**
 * @brief 主窗口类
 * 
 * 负责显示用户界面，处理用户交互，以及调度FFmpeg和Python脚本进行视频处理
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    /**
     * @brief 窗口关闭事件
     * 确保关闭窗口时终止所有正在运行的子进程
     */
    void closeEvent(QCloseEvent *event) override;

private slots:
    /**
     * @brief 添加视频文件 (通过文件对话框)
     */
    void addVideoFiles();
    
    /**
     * @brief 处理拖拽添加的文件
     * @param files 文件路径列表
     */
    void handleDroppedFiles(const QStringList &files);

    /**
     * @brief 选择输出目录
     */
    void selectOutputDir();

    /**
     * @brief 处理队列中的下一个任务
     */
    void processNextTask();

    /**
     * @brief 从队列中删除选中项
     */
    void removeSelectedTask();

    /**
     * @brief 处理提取音频完成
     * @param exitCode 退出代码
     */
    void onExtractAudioFinished(int exitCode);

    /**
     * @brief 处理转录完成
     * @param exitCode 退出代码
     */
    void onTranscribeFinished(int exitCode);

    /**
     * @brief 处理合成视频完成
     * @param exitCode 退出代码
     */
    void onEmbedSubtitleFinished(int exitCode);

private:
    /**
     * @brief 初始化 UI
     */
    void initUI();

    /**
     * @brief 添加视频到队列的内部逻辑
     * @param files 文件路径列表
     */
    void addVideosToQueue(const QStringList &files);

    // UI 控件
    FileDropListWidget *inputListWidget;
    QListWidget *outputListWidget;

    // 配置控件
    QComboBox *engineCombo;
    QComboBox *modelCombo;
    QPushButton *helpButton;
    QLineEdit *outputDirEdit;
    QPushButton *addFilesButton;
    QPushButton *selectOutputDirButton;
    // QPushButton *startButton; // 自动开始，不需要按钮
    QTextEdit *logArea;
    QProgressBar *progressBar;
    QLabel *statusLabel;

    // 数据
    QList<TaskInfo> taskList; // 使用 QList 方便删除
    TaskInfo currentTask;
    bool isProcessing;

    QString tempAudioPath;
    QString outputSubtitlePath;

    // 任务阶段枚举
    enum TaskStage {
        StageNone,
        StageExtract,
        StageTranscribe,
        StageEmbed
    };
    TaskStage currentStage;
    double totalDurationSecs; // 用于计算 FFmpeg 进度
    QProcess *currentProcess; // 当前正在运行的子进程指针

    /**
     * @brief 记录日志
     * @param message 日志信息
     */
    void log(const QString &message);
    
    /**
     * @brief 运行命令 (复用或创建进程)
     * @param program 程序名
     * @param arguments 参数列表
     * @param workDir 工作目录 (可选)
     */
    void runCommand(const QString &program, const QStringList &arguments, const QString &workDir = "");

private slots:
    /**
     * @brief 统一处理进程完成信号
     * @param exitCode 退出代码
     * @param exitStatus 退出状态
     */
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    /**
     * @brief 统一处理标准输出
     */
    void onProcessReadyReadStandardOutput();

    /**
     * @brief 统一处理标准错误
     */
    void onProcessReadyReadStandardError();
};

#endif // MAINWINDOW_H
