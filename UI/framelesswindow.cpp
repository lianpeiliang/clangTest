#include "framelesswindow.h"
#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

FramelessWindow::FramelessWindow(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
}

QFrame *FramelessWindow::createTitleBar()
{
    auto *brand = new QFrame(this);
    m_titleBar = brand;
    brand->setObjectName(QStringLiteral("brandHeader"));
    brand->setFixedHeight(44);
    auto *brandRow = new QHBoxLayout(brand);
    brandRow->setContentsMargins(12, 4, 8, 4);
    brandRow->setSpacing(8);
    auto *logo = new QLabel(brand);
    logo->setPixmap(QIcon(QStringLiteral(":/icons/structhex.png")).pixmap(32, 32));
    brandRow->addWidget(logo);
    auto *brandTitle = new QLabel(QStringLiteral("StructHex"), brand);
    brandTitle->setObjectName(QStringLiteral("brandTitle"));
    auto *brandSubtitle = new QLabel(QStringLiteral("结构体数据编辑器"), brand);
    brandSubtitle->setObjectName(QStringLiteral("brandSubtitle"));
    auto *separator = new QLabel(QStringLiteral("·"), brand);
    separator->setObjectName(QStringLiteral("brandSeparator"));
    brandRow->addWidget(brandTitle);
    brandRow->addWidget(separator);
    brandRow->addWidget(brandSubtitle);
    brandRow->addStretch();
    auto *badge = new QLabel(QStringLiteral("C / C++  ·  HEX"), brand);
    badge->setObjectName(QStringLiteral("brandBadge"));
    brandRow->addWidget(badge);
    brandRow->addSpacing(12);
    auto *minimize = new QPushButton(QStringLiteral("—"), brand);
    minimize->setObjectName(QStringLiteral("minimizeWindowButton"));
    minimize->setToolTip(QStringLiteral("最小化"));
    minimize->setAccessibleName(QStringLiteral("最小化"));
    m_maximizeButton = new QPushButton(QStringLiteral("□"), brand);
    m_maximizeButton->setObjectName(QStringLiteral("maximizeWindowButton"));
    auto *close = new QPushButton(QStringLiteral("×"), brand);
    close->setObjectName(QStringLiteral("closeWindowButton"));
    close->setToolTip(QStringLiteral("关闭"));
    close->setAccessibleName(QStringLiteral("关闭"));
    for (auto *button : {minimize, m_maximizeButton, close}) {
        button->setFixedSize(36, 30);
        brandRow->addWidget(button);
    }
    connect(minimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximizeButton, &QPushButton::clicked, this, [this] {
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(close, &QPushButton::clicked, this, &QWidget::close);
    updateWindowControls();
    return brand;
}

void FramelessWindow::updateWindowControls()
{
    if (!m_maximizeButton)
        return;
    const QString action = isMaximized() ? QStringLiteral("还原") : QStringLiteral("最大化");
    m_maximizeButton->setText(isMaximized() ? QStringLiteral("❐") : QStringLiteral("□"));
    m_maximizeButton->setToolTip(action);
    m_maximizeButton->setAccessibleName(action);
}

void FramelessWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        updateWindowControls();
}

void FramelessWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    if (QApplication::platformName() == QStringLiteral("windows")) {
        const HWND handle = reinterpret_cast<HWND>(winId());
        const LONG_PTR style = GetWindowLongPtr(handle, GWL_STYLE);
        const LONG_PTR required = WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
        if ((style & required) != required) {
            SetWindowLongPtr(handle, GWL_STYLE, style | required);
            SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }
#endif
}

bool FramelessWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#ifdef Q_OS_WIN
    auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_NCCALCSIZE && msg->wParam) {
        *result = 0; // 保留系统窗口操作能力，客户区覆盖原来的系统标题栏。
        return true;
    }
    if (msg->message == WM_GETMINMAXINFO) {
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfo(MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
            auto *info = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            info->ptMaxPosition.x = monitor.rcWork.left - monitor.rcMonitor.left;
            info->ptMaxPosition.y = monitor.rcWork.top - monitor.rcMonitor.top;
            info->ptMaxSize.x = monitor.rcWork.right - monitor.rcWork.left;
            info->ptMaxSize.y = monitor.rcWork.bottom - monitor.rcWork.top;
            info->ptMinTrackSize.x = qRound(minimumWidth() * devicePixelRatioF());
            info->ptMinTrackSize.y = qRound(minimumHeight() * devicePixelRatioF());
            *result = 0;
            return true;
        }
    }
    if (msg->message == WM_NCHITTEST && m_titleBar) {
        RECT rect;
        if (GetWindowRect(msg->hwnd, &rect)) {
            const int x = GET_X_LPARAM(msg->lParam);
            const int y = GET_Y_LPARAM(msg->lParam);
            const int border = qMax(6, qRound(6 * devicePixelRatioF()));
            const bool left = x < rect.left + border;
            const bool right = x >= rect.right - border;
            const bool top = y < rect.top + border;
            const bool bottom = y >= rect.bottom - border;
            if (!isMaximized() && (left || right || top || bottom)) {
                *result = top ? (left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP)
                    : bottom ? (left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM)
                    : left ? HTLEFT : HTRIGHT;
                return true;
            }
            const QPoint local(qRound((x - rect.left) / devicePixelRatioF()),
                               qRound((y - rect.top) / devicePixelRatioF()));
            QWidget *child = childAt(local);
            if (m_titleBar->geometry().contains(local)
                && !qobject_cast<QPushButton *>(child)) {
                *result = HTCAPTION; // 系统负责拖动、双击最大化、还原拖动和贴边吸附。
                return true;
            }
            *result = HTCLIENT;
            return true;
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}
