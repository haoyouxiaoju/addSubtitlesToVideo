#include "MainWindow.h"
#include <QApplication>

/**
 * @brief 主程序入口
 * 
 * 初始化QApplication并显示主窗口
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
