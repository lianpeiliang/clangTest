#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QHash>
#include "headerparser.h"

class QLabel;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// 这一版使用普通 Qt Widgets 控件，解析代码放在 headerparser.cpp 中。
class MainWindow : public QWidget
{
public:
    explicit MainWindow(const QString &filePath, QWidget *parent = nullptr);
    // 键为完整字段路径，例如 address.source；值为已校验的输入文本，保留十六进制等格式。
    const QHash<QString, QString> &fieldValues() const { return m_fieldValues; }
    bool hasInvalidFieldValues() const { return !m_valueErrors.isEmpty(); }

private:
    void reloadDefinition();
    void showSelectedStructure();
    void appendFields(QTreeWidgetItem *parent, const QVector<FieldInfo> &fields,
                      const QString &parentPath, const QHash<QString, QString> &previousValues);
    void updateValueStatus();
    void showBinary();
    void showHexDecoder();
    void exportDefinitionJson();

    QLineEdit *m_filePath;
    QComboBox *m_structName;
    QLabel *m_structureName;
    QLabel *m_summary;
    QLabel *m_sourceInfo;
    QLabel *m_valueStatus;
    QTreeWidget *m_fields;
    QLabel *m_status;
    QPlainTextEdit *m_diagnostics;
    QHash<QString, QString> m_fieldValues;
    QHash<QString, QString> m_valueErrors;
    QString m_valuesSource;
    QPushButton *m_binaryButton;
    QPushButton *m_decodeButton;
    QPushButton *m_exportDefinitionButton;
    StructInfo m_parsedStructure;
    QStringList m_parseDiagnostics;
    bool m_hasStructure = false;
    QVector<ParseResult> m_structures;
    QStringList m_fileDiagnostics;
    QString m_loadedPath;
    bool m_jsonMode = false;
    QString m_headerPath;
    QString m_headerStructName;
    QString m_lastHexInput;
    int m_lastHexByteOrder = 0;
};

#endif
