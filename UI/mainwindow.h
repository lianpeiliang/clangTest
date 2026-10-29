#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "framelesswindow.h"
#include <QHash>
#include "structuresession.h"

// 前置声明：这里只保存控件指针，具体控件的头文件放在 .cpp 中包含。
class QLabel;
class QPlainTextEdit;
class QPushButton;
class FieldEditor;
class DefinitionPanel;
class HexDialogs;

// 主窗口协调定义加载、编辑区及操作按钮；字段值和校验由 FieldEditor 管理。
// 继承 FramelessWindow 以复用标题栏与系统窗口行为，十六进制弹窗交给 HexDialogs 管理。
class MainWindow : public FramelessWindow
{
public:
    // 创建界面并首次加载 filePath（头文件或结构定义 JSON）；parent 是可选的 Qt 父控件。
    explicit MainWindow(const QString &filePath, QWidget *parent = nullptr);

    // 键为完整字段路径，例如 address.source；值为已校验的输入文本，保留十六进制等格式。
    // 返回只读引用，调用方可以查看当前值，但不能通过此接口修改内部容器。
    const QHash<QString, QString> &fieldValues() const;

    // 当前是否有无效输入；返回 true 时，“查看二进制”按钮不可用。
    bool hasInvalidFieldValues() const;

private:
    StructureSession m_session; // 唯一的定义加载与选择状态，界面直接读取此会话。
    DefinitionPanel *m_definitionPanel; // 来源选择面板，只通过信号请求加载、选择或展开操作。
    // 创建操作按钮区并连接信号，构造函数只组织整体布局。
    QWidget *createActionPanel();
    FieldEditor *m_fieldEditor; // 字段编辑区，负责字段值、校验及树形控件。
    HexDialogs *m_hexDialogs; // 弹窗管理器；以本窗口为 Qt 父对象，随窗口一起销毁。
    // 加载文件，更新结构体候选列表；随后显示当前选中项。
    void reloadDefinition(const QString &filePath);

    // 下拉切换只读取已缓存的解析结果，不重新调用 libclang。
    void showSelectedStructure();

    // 根据结构体是否已加载和编辑区是否合法，更新“查看二进制”按钮。
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

    // 选择保存路径并导出当前结构体定义，不包含字段输入值。
    void exportDefinitionJson();

    // Qt 父子对象机制管理这些控件的生命周期；主窗口销毁时，子控件也会销毁。
    QLabel *m_structureName;    // 当前结构体名称标题；加载失败时也在这里显示状态。
    QLabel *m_summary;          // 显示直接字段数量、结构体总大小和对齐要求。
    QLabel *m_status;           // 主操作状态提示，例如加载、导出或生成预览的结果。
    QPlainTextEdit *m_diagnostics; // 错误和警告的详细文本区域，只读显示。
    QString m_valuesSource;               // 文件路径 + 结构体名，用于判断旧值能否复用。
    QPushButton *m_binaryButton;           // “查看二进制”按钮，需有结构体且输入全部合法。
    QPushButton *m_decodeButton;           // “解析十六进制”按钮，加载结构体成功后可用。
    QPushButton *m_importHexButton;        // “导入16进制数据”按钮，允许覆盖当前无效输入。
    QPushButton *m_importHexFileButton;    // “从文件导入16进制数据”按钮，需先加载结构体。
    QPushButton *m_exportDefinitionButton; // “导出结构定义 JSON”按钮，不要求字段输入值全部合法。
};

#endif
