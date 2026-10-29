#ifndef FIELDEDITOR_H
#define FIELDEDITOR_H

#include <QWidget>
#include <QHash>
#include "headerparser.h"

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

// 独立字段编辑区：持有字段树、合法输入值和错误状态，不负责文件或窗口操作。
class FieldEditor : public QWidget
{
    Q_OBJECT
public:
    explicit FieldEditor(QWidget *parent = nullptr);

    // 重建字段树；previousValues 中仍符合字段类型的值会恢复，其他使用初始值。
    void setStructure(const StructInfo &structure,
                      const QHash<QString, QString> &previousValues = QHash<QString, QString>());
    void clear(); // 清空字段、输入值和错误提示，用于切换定义或加载失败。

    // 返回最近通过校验的输入；无效文字仍留在控件中，不覆盖合法值。
    const QHash<QString, QString> &fieldValues() const { return m_fieldValues; }
    bool hasInvalidValues() const { return !m_valueErrors.isEmpty(); }

    // 将解析结果填入当前编辑控件，复用手动输入的校验和提示逻辑。
    void applyValues(const QHash<QString, QString> &values);
    void expandAll();
    void collapseAll();

signals:
    // 编辑、填回或重建后通知调用方更新依赖输入合法性的操作按钮。
    void validityChanged(bool valid);

private:
    void appendFields(QTreeWidgetItem *parent, const QVector<FieldInfo> &fields,
                      const QString &parentPath, const QHash<QString, QString> &previousValues);
    void updateValueStatus();
    QLabel *m_valueStatus; // 输入要求与错误提示。
    QTreeWidget *m_fields; // 字段树及其编辑控件，均由 Qt 父子对象机制管理。
    QHash<QString, QString> m_fieldValues; // 完整字段路径 → 最近的合法输入。
    QHash<QString, QString> m_valueErrors; // 完整字段路径 → 当前输入错误。
};

#endif
