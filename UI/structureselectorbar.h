#ifndef STRUCTURESELECTORBAR_H
#define STRUCTURESELECTORBAR_H

#include <QWidget>
#include <QStringList>

class QComboBox;

// 结构体选择栏：展示候选及其可选状态，并转发选择、展开和折叠意图。
class StructureSelectorBar : public QWidget
{
    Q_OBJECT
public:
    explicit StructureSelectorBar(QWidget *parent = nullptr);
    void setDefinitions(const QStringList &names, int selectedIndex, bool jsonMode); // 静默刷新候选。
    int count() const; // 供面板显示候选数量，不暴露内部控件。

signals:
    void structureSelected(int index);
    void expandRequested();
    void collapseRequested();

private:
    QComboBox *m_structName;
};

#endif
