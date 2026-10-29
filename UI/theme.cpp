#include "theme.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

void applyApplicationTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor("#F7F4FB"));
    palette.setColor(QPalette::WindowText, QColor("#342A43"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#FBF9FE"));
    palette.setColor(QPalette::Text, QColor("#342A43"));
    palette.setColor(QPalette::Button, Qt::white);
    palette.setColor(QPalette::ButtonText, QColor("#644783"));
    palette.setColor(QPalette::Highlight, QColor("#E9DDF8"));
    palette.setColor(QPalette::HighlightedText, QColor("#502A80"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#A399AF"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#A399AF"));
    app.setPalette(palette);
#ifdef Q_OS_WIN
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
#endif
    QFile sheet(QStringLiteral(":/styles/structhex.qss"));
    if (!sheet.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot load StructHex stylesheet:" << sheet.errorString();
        return;
    }
    app.setStyleSheet(QString::fromUtf8(sheet.readAll()));
}
