#ifndef FRAMELESSWINDOW_H
#define FRAMELESSWINDOW_H

#include <QWidget>

// 头文件中只保存控件指针，具体控件定义放在实现文件中。
class QFrame;
class QPushButton;

// 只负责标题栏与原生窗口行为，不依赖结构体、文件或字段数据。
// MainWindow 继承本类，复用拖动、缩放、最大化及窗口按钮的处理。
class FramelessWindow : public QWidget
{
public:
    // 启用无边框窗口；parent 由 Qt 父子对象机制管理。
    explicit FramelessWindow(QWidget *parent = nullptr);

protected:
    // 创建紫色标题栏及窗口按钮；派生类构建界面时调用一次，再加入自己的布局。
    QFrame *createTitleBar();

    // 处理 Windows 原生消息：确定拖动/缩放区域，并限制最大化范围为显示器工作区。
    // 返回 true 表示消息已处理，result 保存交给系统的处理结果。
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

    // 显示窗口时补齐 Windows 的缩放、最小化和最大化能力。
    void showEvent(QShowEvent *event) override;

    // 窗口状态变化后，同步最大化/还原按钮的图形和提示。
    void changeEvent(QEvent *event) override;

private:
    // 根据 isMaximized() 更新按钮显示及无障碍名称。
    void updateWindowControls();
    QFrame *m_titleBar = nullptr;           // 标题栏区域，用于原生鼠标命中判断。
    QPushButton *m_maximizeButton = nullptr; // 同一个按钮承担最大化和还原操作。
};

#endif
