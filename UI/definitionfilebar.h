#ifndef DEFINITIONFILEBAR_H
#define DEFINITIONFILEBAR_H

#include <QWidget>

class QLineEdit;

// 定义文件操作栏：管理待加载路径，将打开、回车和重载统一转换为加载请求。
class DefinitionFileBar : public QWidget
{
    Q_OBJECT
public:
    explicit DefinitionFileBar(const QString &filePath, QWidget *parent = nullptr);

signals:
    void loadRequested(const QString &filePath); // 只表达用户意图，不校验或解析文件。

private:
    QLineEdit *m_filePath; // 编辑中的路径与当前会话的已加载路径相互独立。
};

#endif
