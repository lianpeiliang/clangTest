#include "UI/mainwindow.h"
#include "UI/theme.h"
#include <QApplication>
#include <QFrame>
#include <QPushButton>
#include <QTextStream>
#include <QKeyEvent>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

#define CHECK(c) do { if (!(c)) { QTextStream(stderr) << "FAIL " << __LINE__ << ": " << #c << '\n'; return 1; } } while (false)

class TestWindow : public MainWindow {
public:
    using MainWindow::MainWindow;
    using MainWindow::nativeEvent;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    applyApplicationTheme(app);
    TestWindow window(QStringLiteral(MSG_HEADER_PATH));
    window.show();
    app.processEvents();
    CHECK(window.windowFlags() & Qt::FramelessWindowHint);
    auto *title = window.findChild<QFrame *>("brandHeader");
    auto *maximize = window.findChild<QPushButton *>("maximizeWindowButton");
    auto *minimize = window.findChild<QPushButton *>("minimizeWindowButton");
    auto *close = window.findChild<QPushButton *>("closeWindowButton");
    CHECK(title && maximize && minimize && close);
#ifdef Q_OS_WIN
    CHECK(QApplication::platformName() == "windows");
    const HWND handle = reinterpret_cast<HWND>(window.winId());
    const LONG_PTR style = GetWindowLongPtr(handle, GWL_STYLE);
    CHECK((style & WS_THICKFRAME) && (style & WS_MAXIMIZEBOX) && (style & WS_MINIMIZEBOX));
    RECT rect;
    CHECK(GetWindowRect(handle, &rect));
    // WM_NCCALCSIZE 的简化形式也必须保留整块客户区，不能回退到系统边框。
    RECT calculated = rect;
    SendMessage(handle, WM_NCCALCSIZE, FALSE, reinterpret_cast<LPARAM>(&calculated));
    CHECK(EqualRect(&calculated, &rect));
    const QRect titleGeometry = title->geometry();
    CHECK(titleGeometry.topLeft() == QPoint(0, 0));
    for (int cycle = 0; cycle < 3; ++cycle) {
        SendMessage(handle, WM_NCACTIVATE, FALSE, 0);
        SendMessage(handle, WM_NCACTIVATE, TRUE, 0);
        SendMessage(handle, WM_NCPAINT, 1, 0);
        SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Tab, Qt::ControlModifier);
        QApplication::sendEvent(&window, &press);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Tab, Qt::ControlModifier);
        QApplication::sendEvent(&window, &release);
        app.processEvents();
        CHECK(title->geometry() == titleGeometry);
        RECT client;
        CHECK(GetClientRect(handle, &client));
        POINT origin = {0, 0};
        CHECK(ClientToScreen(handle, &origin));
        RECT outer;
        CHECK(GetWindowRect(handle, &outer));
        CHECK(origin.x == outer.left && origin.y == outer.top);
        CHECK(client.right == outer.right - outer.left && client.bottom == outer.bottom - outer.top);
    }
    auto hit = [&](int x, int y) {
        MSG message = {};
        message.hwnd = handle;
        message.message = WM_NCHITTEST;
        message.lParam = MAKELPARAM(x, y);
        long result = -1;
        window.nativeEvent("windows_generic_MSG", &message, &result);
        return result;
    };
    const int midX = (rect.left + rect.right) / 2;
    const int midY = (rect.top + rect.bottom) / 2;
    CHECK(hit(rect.left + 1, midY) == HTLEFT);
    CHECK(hit(rect.right - 1, midY) == HTRIGHT);
    CHECK(hit(midX, rect.top + 1) == HTTOP);
    CHECK(hit(midX, rect.bottom - 1) == HTBOTTOM);
    CHECK(hit(rect.left + 1, rect.top + 1) == HTTOPLEFT);
    CHECK(hit(rect.right - 1, rect.top + 1) == HTTOPRIGHT);
    CHECK(hit(rect.left + 1, rect.bottom - 1) == HTBOTTOMLEFT);
    CHECK(hit(rect.right - 1, rect.bottom - 1) == HTBOTTOMRIGHT);
    auto nativePoint = [&](const QPoint &point) {
        return QPoint(rect.left + qRound(point.x() * window.devicePixelRatioF()),
                      rect.top + qRound(point.y() * window.devicePixelRatioF()));
    };
    const QPoint drag = nativePoint(title->geometry().center());
    CHECK(hit(drag.x(), drag.y()) == HTCAPTION);
    for (auto *button : {maximize, minimize, close}) {
        const QPoint point = nativePoint(button->mapTo(&window, button->rect().center()));
        CHECK(hit(point.x(), point.y()) == HTCLIENT);
    }
    SendMessage(handle, WM_NCLBUTTONDBLCLK, HTCAPTION, MAKELPARAM(drag.x(), drag.y()));
    app.processEvents();
    CHECK(window.isMaximized());
    CHECK(maximize->toolTip() == QStringLiteral("还原"));
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    CHECK(GetMonitorInfo(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor));
    CHECK(GetWindowRect(handle, &rect));
    CHECK(rect.left == monitor.rcWork.left && rect.top == monitor.rcWork.top);
    CHECK(rect.right == monitor.rcWork.right && rect.bottom == monitor.rcWork.bottom);
#else
    maximize->click();
    app.processEvents();
#endif
    maximize->click();
    app.processEvents();
    CHECK(!window.isMaximized() && maximize->toolTip() == QStringLiteral("最大化"));
    minimize->click();
    app.processEvents();
    CHECK(window.isMinimized());
    window.showNormal();
    app.processEvents();
    if (app.arguments().size() > 1)
        CHECK(window.grab().save(app.arguments().at(1)));
    close->click();
    CHECK(!window.isVisible());
    QTextStream(stdout) << "PASS: custom title, native drag/resize hit tests, controls, double click, taskbar bounds and restore\n";
    return 0;
}
