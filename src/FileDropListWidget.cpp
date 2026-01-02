#include "FileDropListWidget.h"

FileDropListWidget::FileDropListWidget(QWidget *parent)
    : QListWidget(parent)
{
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DropOnly);
}

void FileDropListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileDropListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileDropListWidget::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QStringList files;
        QList<QUrl> urlList = mimeData->urls();
        for (const QUrl &url : urlList) {
            QString localFile = url.toLocalFile();
            QFileInfo fileInfo(localFile);
            // 简单过滤，只接受视频文件后缀 (可根据需要扩展)
            QString suffix = fileInfo.suffix().toLower();
            if (suffix == "mp4" || suffix == "avi" || suffix == "mkv" || suffix == "mov" || suffix == "flv" || suffix == "wmv") {
                files.append(localFile);
            }
        }
        
        if (!files.isEmpty()) {
            emit filesDropped(files);
        }
        event->acceptProposedAction();
    }
}
