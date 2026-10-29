#ifndef THEME_H
#define THEME_H

class QApplication;

// 主程序和界面回归检查使用同一份内嵌主题。
// 在创建窗口前调用，统一设置 Fusion 控件风格、调色板、字体和资源中的 QSS。
// 该设置作用于整个 QApplication，后续创建的弹窗也会沿用主题。
void applyApplicationTheme(QApplication &app);

#endif
