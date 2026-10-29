#ifndef HEXDIALOGS_H
#define HEXDIALOGS_H

#include <QObject>
#include <QHash>
#include <functional>
#include "headerparser.h"
class QWidget;

// 管理十六进制弹窗及其输入记忆；只通过回调请求主界面更新，不持有字段编辑控件。
class HexDialogs : public QObject
{
public:
    // 填回回调：键为完整字段路径，例如 address.source；值为通过校验的输入文字。
    using ApplyValues = std::function<void(const QHash<QString, QString> &)>;

    // 导入回调：第一个参数为十六进制文本；成功返回 true，失败时将原因写入第二个参数。
    using ImportText = std::function<bool(const QString &, QString &)>;

    // owner 必须是有效的主窗口；本对象和弹窗均交由该窗口管理生命周期。
    explicit HexDialogs(QWidget *owner);

    // 显示字节快照，提供复制和文件导出；sourcePath 用于建议保存位置。
    void showPreview(const StructInfo &structure, const QByteArray &bytes, const QString &sourcePath);

    // 打开解析弹窗，先预览结果，用户确认填回后才调用 applyValues。
    void showDecoder(const StructInfo &structure, const ApplyValues &applyValues);

    // 打开粘贴导入弹窗；确认时调用 importText，失败显示原因，成功关闭弹窗。
    void showImporter(const StructInfo &structure, const ImportText &importText);

    // 记住最近输入或成功从文件导入的文字，供下次打开弹窗时恢复。
    void rememberInput(const QString &text) { m_lastHexInput = text; }

    // 结构定义切换或重载时关闭解析/导入弹窗，避免将旧布局的结果填回新结构体。
    // 已打开的二进制预览仍保留自己的字节快照。
    void closeEditors();

private:
    QWidget *m_owner;            // 弹窗父窗口；不由本类删除。
    QString m_lastHexInput;      // 仅在当前进程内存中保存的最近一次输入。
    int m_lastHexByteOrder = 0;  // 解析弹窗的字节序选项：0 为小端，1 为大端。
};

#endif
