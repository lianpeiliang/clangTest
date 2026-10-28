#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // GUI 程序使用 QApplication；事件循环负责窗口显示和按钮响应。
    QApplication app(argc, argv);
    // 便携包优先使用程序旁边的示例，不依赖构建时的本机源码路径。
    QString defaultPath = QDir(app.applicationDirPath()).filePath(QStringLiteral("TestHeader/MsgHeader.h"));
    if (!QFileInfo::exists(defaultPath))
        defaultPath = QStringLiteral(MSG_HEADER_PATH);
    const QString filePath = app.arguments().size() > 1
        ? app.arguments().at(1) : defaultPath;
    MainWindow window(filePath);
    window.show();
    return app.exec();
}
