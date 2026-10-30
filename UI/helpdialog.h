#ifndef HELPDIALOG_H
#define HELPDIALOG_H

class QWidget;

// 打开非模态帮助窗口；同一主窗口只保留一份，关闭后释放。
void showHelpDialog(QWidget *owner);

#endif
