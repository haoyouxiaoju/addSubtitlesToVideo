#ifndef FILEDROPLISTWIDGET_H
#define FILEDROPLISTWIDGET_H

#include <QListWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>

/**
 * @brief 支持文件拖拽的 ListWidget
 */
class FileDropListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit FileDropListWidget(QWidget *parent = nullptr);

signals:
    /**
     * @brief 当文件被拖入时触发
     * @param filePaths 文件绝对路径列表
     */
    void filesDropped(const QStringList &filePaths);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

#endif // FILEDROPLISTWIDGET_H
