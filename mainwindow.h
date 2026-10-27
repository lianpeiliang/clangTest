#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QHash>
#include "headerparser.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

// 这一版使用普通 Qt Widgets 控件，解析代码放在 headerparser.cpp 中。
class MainWindow : public QWidget
{
public:
    explicit MainWindow(const QString &filePath, QWidget *parent = nullptr);
    // 键为相对所选结构体的完整字段路径，例如 address.source；值为已校验的文本。
    const QHash<QString, QString> &fieldValues() const { return m_fieldValues; }
    bool hasInvalidFieldValues() const { return !m_valueErrors.isEmpty(); }

private:
    void reloadHeader();
    void appendFields(QTreeWidgetItem *parent, const QVector<FieldInfo> &fields,
                      const QString &parentPath, const QHash<QString, QString> &previousValues);
    void updateValueStatus();

    QLineEdit *m_filePath;
    QLineEdit *m_structName;
    QLabel *m_structureName;
    QLabel *m_summary;
    QLabel *m_valueStatus;
    QTreeWidget *m_fields;
    QLabel *m_status;
    QPlainTextEdit *m_diagnostics;
    QHash<QString, QString> m_fieldValues;
    QHash<QString, QString> m_valueErrors;
    QString m_valuesSource;
};

#endif
