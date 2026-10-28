#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QHash>
#include "headerparser.h"

// 前置声明：这里只保存控件指针，具体控件的头文件放在 .cpp 中包含。
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
    // 加载文件，更新结构体候选列表；随后显示当前选中项。
    void reloadDefinition();
    // 下拉切换只读取已缓存的解析结果，不重新调用 libclang。
    void showSelectedStructure();
    // 递归把字段树变成界面树，同时创建字段值的编辑控件。
    void appendFields(QTreeWidgetItem *parent, const QVector<FieldInfo> &fields,
                      const QString &parentPath, const QHash<QString, QString> &previousValues);
    // 汇总输入错误，并更新“查看二进制”按钮是否可用。
    void updateValueStatus();
    void showBinary();
    void showHexDecoder();
    void exportDefinitionJson();

    // Qt 父子对象机制管理这些控件的生命周期；主窗口销毁时，子控件也会销毁。
    QLineEdit *m_filePath;       // 文件路径输入框。
    QComboBox *m_structName;    // 自动发现的结构体下拉列表。
    QLabel *m_structureName;
    QLabel *m_summary;
    QLabel *m_sourceInfo;
    QLabel *m_valueStatus;
    QTreeWidget *m_fields;
    QLabel *m_status;
    QPlainTextEdit *m_diagnostics;
    QHash<QString, QString> m_fieldValues; // 字段路径 → 最近一次通过校验的输入文本。
    QHash<QString, QString> m_valueErrors; // 字段路径 → 当前输入错误；为空表示全部合法。
    QString m_valuesSource;               // 文件路径 + 结构体名，用于判断旧值能否复用。
    QPushButton *m_binaryButton;
    QPushButton *m_decodeButton;
    QPushButton *m_exportDefinitionButton;
    StructInfo m_parsedStructure; // 当前正在显示、编辑和导出的结构体。
    QStringList m_parseDiagnostics;
    bool m_hasStructure = false;
    QVector<ParseResult> m_structures; // 本次文件加载得到的所有候选，顺序与下拉框一致。
    QStringList m_fileDiagnostics;   // 整个文件的诊断，与单个结构体的布局错误分开。
    QString m_loadedPath;            // 已加载的文件路径，不随输入框中未确认的文字改变。
    bool m_jsonMode = false;         // true：从 JSON 读取；false：用 libclang 解析头文件。
    QString m_headerPath;           // 记住上次头文件及其选择，便于从 JSON 切回时恢复。
    QString m_headerStructName;
    QString m_lastHexInput;         // 只在当前窗口内存中记住上次的十六进制输入。
    int m_lastHexByteOrder = 0;
};

#endif
