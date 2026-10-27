#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

// 这一版使用普通 Qt Widgets 控件，解析代码放在 headerparser.cpp 中。
class MainWindow : public QWidget
{
public:
    explicit MainWindow(const QString &filePath, QWidget *parent = nullptr);

private:
    void reloadHeader();

    QLineEdit *m_filePath;
    QLineEdit *m_structName;
    QLabel *m_structureName;
    QLabel *m_summary;
    QTableWidget *m_fields;
    QLabel *m_status;
    QPlainTextEdit *m_diagnostics;
};

#endif
