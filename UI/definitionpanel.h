#ifndef DEFINITIONPANEL_H
#define DEFINITIONPANEL_H

#include <QFrame>
#include <QStringList>

class QLabel;
class QLineEdit;
class QComboBox;

// 定义选择面板：只展示文件来源和结构体候选，通过信号请求操作，不解析文件。
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
    void structureSelected(int index); // 用户切换候选；只传递索引，不直接修改会话。
    void expandRequested();
    void collapseRequested();

private:
    QLineEdit *m_filePath;    // 用户输入的路径，未确认时不影响当前已加载定义。
    QComboBox *m_structName; // 候选的显示和选择控件。
    QLabel *m_sourceInfo;   // 来源及加载状态说明。
};

#endif
