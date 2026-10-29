#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include "mainwindow.h"

// 程序入口。阅读顺序：创建应用 → 确定头文件路径 → 创建窗口 → 进入事件循环。
int main(int argc, char *argv[])
{
    // GUI 程序使用 QApplication；事件循环负责窗口显示和按钮响应。
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("StructHex"));
    app.setApplicationDisplayName(QStringLiteral("StructHex · 结构体数据编辑器"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/structhex.png")));
    // 便携包优先使用程序旁边的示例，不依赖构建时的本机源码路径。
    QString defaultPath = QDir(app.applicationDirPath()).filePath(QStringLiteral("TestHeader/MsgHeader.h"));
    if (!QFileInfo::exists(defaultPath))
        defaultPath = QStringLiteral(MSG_HEADER_PATH);
    // 命令行第 0 项是程序自身的路径；第 1 项才是用户传入的头文件或 JSON 路径。
    const QString filePath = app.arguments().size() > 1
        ? app.arguments().at(1) : defaultPath;
    // 构造函数负责建立界面并首次加载文件，show() 让窗口可见。
    MainWindow window(filePath);
    window.show();
    // exec() 持续处理点击、键盘等事件；关闭最后一个窗口后，事件循环通常结束。
    return app.exec();
}
