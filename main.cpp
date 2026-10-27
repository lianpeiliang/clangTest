#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // GUI 程序使用 QApplication；事件循环负责窗口显示和按钮响应。
    QApplication app(argc, argv);
    const QString filePath = app.arguments().size() > 1
        ? app.arguments().at(1) : QStringLiteral(MSG_HEADER_PATH);
    MainWindow window(filePath);
    window.show();
    return app.exec();
}
