#ifndef DEFINITIONPANEL_H
#define DEFINITIONPANEL_H

#include <QFrame>
#include <QStringList>

class QLabel;
class DefinitionFileBar;
class StructureSelectorBar;

// 定义选择面板：组合文件操作栏、结构体选择栏及来源提示，对外统一转发操作信号。
class DefinitionPanel : public QFrame
{
    Q_OBJECT
public:
    explicit DefinitionPanel(const QString &filePath, QWidget *parent = nullptr);

    // 由会话结果更新候选；刷新时不会发出 structureSelected，避免重复处理。
    void setDefinitions(const QStringList &names, int selectedIndex, bool jsonMode);
    void setSourceState(bool jsonMode, bool loaded); // 刷新来源类型、候选数和加载状态提示。

signals:
    void loadRequested(const QString &filePath); // 打开、回车或重新加载；路径可尚未校验。
    void exportRequested(); // 导出当前定义，由主窗口协调保存。
    void structureSelected(int index); // 用户切换候选；只传递索引，不直接修改会话。
    void expandRequested();
    void collapseRequested();

private:
    DefinitionFileBar *m_fileBar;
    StructureSelectorBar *m_selector; // 候选管理交由选择栏，面板只负责组合和转发。
    QLabel *m_sourceInfo;   // 来源及加载状态说明。
};

#endif
