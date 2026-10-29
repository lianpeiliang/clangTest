#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "framelesswindow.h"
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
class HexDialogs;

// 主窗口负责接收操作、展示结构体和管理输入值；解析及数据转换由其他模块完成。
// 继承 FramelessWindow 以复用标题栏与系统窗口行为，十六进制弹窗交给 HexDialogs 管理。
class MainWindow : public FramelessWindow
{
public:
    // 创建界面并首次加载 filePath（头文件或结构定义 JSON）；parent 是可选的 Qt 父控件。
    explicit MainWindow(const QString &filePath, QWidget *parent = nullptr);

    // 键为完整字段路径，例如 address.source；值为已校验的输入文本，保留十六进制等格式。
    // 返回只读引用，调用方可以查看当前值，但不能通过此接口修改内部容器。
    const QHash<QString, QString> &fieldValues() const { return m_fieldValues; }

    // 当前是否有无效输入；返回 true 时，“查看二进制”按钮不可用。
    bool hasInvalidFieldValues() const { return !m_valueErrors.isEmpty(); }

private:
    HexDialogs *m_hexDialogs; // 弹窗管理器；以本窗口为 Qt 父对象，随窗口一起销毁。
    // 加载文件，更新结构体候选列表；随后显示当前选中项。
    void reloadDefinition();

    // 下拉切换只读取已缓存的解析结果，不重新调用 libclang。
    void showSelectedStructure();

    // 递归把字段树变成界面树，同时创建字段值的编辑控件。
    // parent 是界面父节点，fields 是当前层成员，parentPath 用于拼接路径，previousValues 用于恢复合法旧值。
    void appendFields(QTreeWidgetItem *parent, const QVector<FieldInfo> &fields,
                      const QString &parentPath, const QHash<QString, QString> &previousValues);

    // 汇总输入错误，并更新“查看二进制”按钮是否可用。
    void updateValueStatus();

    // 编码当前字段值，弹窗显示十六进制快照，并提供复制和导出文件按钮。
    void showBinary();

    // 打开十六进制解码弹窗；先预览结果，由用户点击按钮后再填回可编辑字段。
    void showHexDecoder();

    // 输入完整的小端十六进制数据，校验成功后直接导入当前结构体并关闭弹窗。
    void showHexImporter();

    // 读取十六进制文本文件，校验成功后直接更新主界面的可编辑字段。
    void importHexFile();

    // 粘贴导入与文件导入共用；失败时保留全部字段值，并返回错误原因。
    bool importHexText(const QString &text, QString &error);

    // 通过当前树上的编辑控件更新值，复用手动输入的校验和错误状态同步。
    void applyDecodedValues(const QHash<QString, QString> &values);

    // 选择保存路径并导出当前结构体定义，不包含字段输入值。
    void exportDefinitionJson();

    // Qt 父子对象机制管理这些控件的生命周期；主窗口销毁时，子控件也会销毁。
    QLineEdit *m_filePath;       // 文件路径输入框。
    QComboBox *m_structName;    // 自动发现的结构体下拉列表。
    QLabel *m_structureName;    // 当前结构体名称标题；加载失败时也在这里显示状态。
    QLabel *m_summary;          // 显示直接字段数量、结构体总大小和对齐要求。
    QLabel *m_sourceInfo;       // 显示文件来源、发现的结构体数量及目标平台等信息。
    QLabel *m_valueStatus;      // 显示字段输入规则，或当前无效输入的数量和原因。
    QTreeWidget *m_fields;      // 主字段树，展示嵌套成员、布局信息和字段值编辑控件。
    QLabel *m_status;           // 主操作状态提示，例如加载、导出或生成预览的结果。
    QPlainTextEdit *m_diagnostics; // 错误和警告的详细文本区域，只读显示。
    QHash<QString, QString> m_fieldValues; // 字段路径 → 最近一次通过校验的输入文本。
    QHash<QString, QString> m_valueErrors; // 字段路径 → 当前输入错误；为空表示全部合法。
    QString m_valuesSource;               // 文件路径 + 结构体名，用于判断旧值能否复用。
    QPushButton *m_binaryButton;           // “查看二进制”按钮，需有结构体且输入全部合法。
    QPushButton *m_decodeButton;           // “解析十六进制”按钮，加载结构体成功后可用。
    QPushButton *m_importHexButton;        // “导入16进制数据”按钮，允许覆盖当前无效输入。
    QPushButton *m_importHexFileButton;    // “从文件导入16进制数据”按钮，需先加载结构体。
    QPushButton *m_exportDefinitionButton; // “导出结构定义 JSON”按钮，不要求字段输入值全部合法。
    StructInfo m_parsedStructure; // 当前正在显示、编辑和导出的结构体。
    QStringList m_parseDiagnostics; // 当前所选结构体及其来源文件的加载诊断，用于恢复诊断显示。
    bool m_hasStructure = false;    // 当前是否有成功加载的结构体，决定后续按钮是否能启用。
    QVector<ParseResult> m_structures; // 本次文件加载得到的所有候选，顺序与下拉框一致。
    QStringList m_fileDiagnostics;   // 整个文件的诊断，与单个结构体的布局错误分开。
    QString m_loadedPath;            // 已加载的文件路径，不随输入框中未确认的文字改变。
    bool m_jsonMode = false;         // true：从 JSON 读取；false：用 libclang 解析头文件。
    QString m_headerPath;           // 上次选择的头文件路径，便于从 JSON 切回时恢复选择。
    QString m_headerStructName;     // 上次在该头文件中选中的结构体名称。
};

#endif
