#include "screen_player_window.h"

#include "video_player_widget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QCursor>
#include <QScreen>
#include <QSize>
#include <QtMath>
#include <windows.h>

namespace
{
constexpr int kTitleBarHeight = 30;
constexpr int kTitleButtonHeight = 24;
constexpr int kResizeBorder = 8;

bool isInResizeBorder(const QPoint &pos, int width, int height, LONG *hitTest)
{
    const bool left = pos.x() <= kResizeBorder;
    const bool right = pos.x() >= width - kResizeBorder;
    const bool top = pos.y() <= kResizeBorder;
    const bool bottom = pos.y() >= height - kResizeBorder;

    if (top && left) {
        *hitTest = HTTOPLEFT;
        return true;
    }
    if (top && right) {
        *hitTest = HTTOPRIGHT;
        return true;
    }
    if (bottom && left) {
        *hitTest = HTBOTTOMLEFT;
        return true;
    }
    if (bottom && right) {
        *hitTest = HTBOTTOMRIGHT;
        return true;
    }
    if (left) {
        *hitTest = HTLEFT;
        return true;
    }
    if (right) {
        *hitTest = HTRIGHT;
        return true;
    }
    if (top) {
        *hitTest = HTTOP;
        return true;
    }
    if (bottom) {
        *hitTest = HTBOTTOM;
        return true;
    }

    return false;
}
}

namespace
{
QIcon iconForPath(const char *path)
{
    return QIcon(QString::fromLatin1(path));
}
}

ScreenPlayerWindow::ScreenPlayerWindow(const ScreenEntry &entry, QWidget *parent)
    : QWidget(parent), m_entry(entry)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowTitle(QStringLiteral("Qt Multi Player - Screen %1").arg(entry.screenIndex));
    setWindowIcon(iconForPath(":/assets/icons/app.svg"));
    setMinimumSize(960, 540);

    auto *rootLayout = new QGridLayout(this);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(6);
    m_grid = rootLayout;

    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(kTitleBarHeight);
    auto *titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(8, 2, 4, 2);
    titleLayout->setSpacing(6);

    m_titleLabel = new QLabel(windowTitle(), m_titleBar);
    m_minButton = new QPushButton(m_titleBar);
    m_fullScreenButton = new QPushButton(m_titleBar);
    m_closeButton = new QPushButton(m_titleBar);
    m_minButton->setObjectName(QStringLiteral("titleMinButton"));
    m_fullScreenButton->setObjectName(QStringLiteral("titleFullscreenButton"));
    m_closeButton->setObjectName(QStringLiteral("titleCloseButton"));
    m_minButton->setToolTip(QStringLiteral("Minimize"));
    m_fullScreenButton->setToolTip(QStringLiteral("Toggle Fullscreen"));
    m_closeButton->setToolTip(QStringLiteral("Close"));
    m_minButton->setIcon(iconForPath(":/assets/icons/title_min.svg"));
    m_closeButton->setIcon(iconForPath(":/assets/icons/title_close.svg"));
    m_minButton->setIconSize(QSize(14, 14));
    m_fullScreenButton->setIconSize(QSize(14, 14));
    m_closeButton->setIconSize(QSize(14, 14));
    m_minButton->setFixedSize(28, kTitleButtonHeight);
    m_fullScreenButton->setFixedSize(34, kTitleButtonHeight);
    m_closeButton->setFixedSize(28, kTitleButtonHeight);

    connect(m_minButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_fullScreenButton, &QPushButton::clicked, this, [this] { toggleFullScreen(nullptr, !isFullScreen()); });
    connect(m_closeButton, &QPushButton::clicked, this, &ScreenPlayerWindow::closeWindow);
    m_titleBar->installEventFilter(this);
    m_titleLabel->installEventFilter(this);

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch(1);
    titleLayout->addWidget(m_minButton);
    titleLayout->addWidget(m_fullScreenButton);
    titleLayout->addWidget(m_closeButton);
    rootLayout->addWidget(m_titleBar, 0, 0, 1, -1);
    rootLayout->setRowStretch(0, 0);

    const int cols = computeColumnCount();
    for (int i = 0; i < entry.videos.size(); ++i) {
        auto *panel = new VideoPlayerWidget(entry.videos.at(i), this);
        connect(panel, &VideoPlayerWidget::requestToggleFullWindow, this, &ScreenPlayerWindow::toggleFullWindow);
        connect(panel, &VideoPlayerWidget::requestToggleFullScreen, this, &ScreenPlayerWindow::toggleFullScreen);

        const int row = i / cols + 1;
        const int col = i % cols;
        m_grid->addWidget(panel, row, col);
        m_grid->setRowStretch(row, 1);
        m_grid->setColumnStretch(col, 1);
        m_positions.insert(panel, QPoint(row, col));
        m_panels.push_back(panel);
    }

    setStyleSheet(QStringLiteral(
        "ScreenPlayerWindow, QWidget { background: #1b1b1b; color: white; font-family: 'Microsoft YaHei'; }"
        "QPushButton { min-width: 28px; padding: 2px 8px; }"
        "QPushButton#titleMinButton, QPushButton#titleFullscreenButton, QPushButton#titleCloseButton {"
        "  min-width: 0px; border-radius: 4px; padding: 0; background: rgba(255,255,255,0.02);"
        "}"
        "QPushButton#titleMinButton:hover, QPushButton#titleFullscreenButton:hover, QPushButton#titleCloseButton:hover {"
        "  background: rgba(255,255,255,0.14);"
        "}"
        "QLabel { font-size: 12px; }"));

    updateWindowButtons();
}

void ScreenPlayerWindow::placeOnScreen(const QRect &screenGeometry)
{
    const int width = qMax(960, screenGeometry.width() * 2 / 3);
    const int height = qMax(540, screenGeometry.height() * 2 / 3);
    const int x = screenGeometry.x() + (screenGeometry.width() - width) / 2;
    const int y = screenGeometry.y() + 30;
    setGeometry(x, y, width, height);
}

void ScreenPlayerWindow::stop()
{
    for (VideoPlayerWidget *panel : std::as_const(m_panels)) {
        panel->stop();
    }
}

void ScreenPlayerWindow::toggleFullWindow(VideoPlayerWidget *panel, bool enabled)
{
    if (!panel) {
        return;
    }

    if (enabled) {
        if (m_fullWindowPanel == panel) {
            return;
        }

        m_fullWindowPanel = panel;
        for (VideoPlayerWidget *candidate : std::as_const(m_panels)) {
            if (candidate != panel) {
                candidate->hide();
            }
        }

        m_grid->removeWidget(panel);
        const int rowCount = contentRowCount();
        const int columnCount = contentColumnCount();
        for (int row = 1; row <= rowCount; ++row) {
            m_grid->setRowStretch(row, 0);
        }
        for (int col = 0; col < columnCount; ++col) {
            m_grid->setColumnStretch(col, 1);
        }
        m_grid->setRowStretch(1, 1);
        m_grid->addWidget(panel, 1, 0, rowCount, columnCount);
        panel->raise();
        panel->setFullWindowActive(true);
    } else {
        if (m_fullWindowPanel != panel) {
            return;
        }

        m_grid->removeWidget(panel);
        const QPoint pos = m_positions.value(panel);
        m_grid->addWidget(panel, pos.x(), pos.y());
        const int rowCount = contentRowCount();
        const int columnCount = contentColumnCount();
        for (int row = 1; row <= rowCount; ++row) {
            m_grid->setRowStretch(row, 1);
        }
        for (int col = 0; col < columnCount; ++col) {
            m_grid->setColumnStretch(col, 1);
        }
        for (VideoPlayerWidget *candidate : std::as_const(m_panels)) {
            candidate->show();
        }
        m_fullWindowPanel = nullptr;
        panel->setFullWindowActive(false);
    }
}

void ScreenPlayerWindow::toggleFullScreen(VideoPlayerWidget *panel, bool enabled)
{
    Q_UNUSED(panel);

    if (enabled) {
        if (!isFullScreen()) {
            m_normalGeometry = geometry();
            showFullScreen();
        }
    } else {
        if (isFullScreen()) {
            showNormal();
            if (m_normalGeometry.isValid()) {
                setGeometry(m_normalGeometry);
            }
        }
    }

    for (VideoPlayerWidget *candidate : std::as_const(m_panels)) {
        candidate->setFullScreenActive(isFullScreen());
    }
    updateWindowButtons();
}

void ScreenPlayerWindow::closeWindow()
{
    close();
}

void ScreenPlayerWindow::closeEvent(QCloseEvent *event)
{
    stop();
    if (!m_closeNotified) {
        m_closeNotified = true;
        emit closedByUser();
    }
    QWidget::closeEvent(event);
}

bool ScreenPlayerWindow::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_titleBar || watched == m_titleLabel) && !isFullScreen()) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            toggleFullScreen(nullptr, !isFullScreen());
            return true;
        }

        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const QPoint localPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());
                if (isDragArea(localPos)) {
#ifdef QT_MULTIPLAYER_SMOOTH_WINDOW_DRAG
                    m_dragging = true;
                    m_dragOffset = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
                    return true;
#endif
                }
            }
        }

        if (event->type() == QEvent::MouseMove && m_dragging) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            move(mouseEvent->globalPosition().toPoint() - m_dragOffset);
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragging = false;
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

bool ScreenPlayerWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (isFullScreen() || eventType != "windows_generic_MSG") {
        return QWidget::nativeEvent(eventType, message, result);
    }

    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_NCHITTEST) {
        const QPoint pos = mapFromGlobal(QCursor::pos());
        LONG hitTest = HTCLIENT;

        if (isInResizeBorder(pos, width(), height(), &hitTest)) {
            *result = hitTest;
            return true;
        }

#ifndef QT_MULTIPLAYER_SMOOTH_WINDOW_DRAG
        if (isDragArea(pos)) {
            *result = HTCAPTION;
            return true;
        }
#endif
    }

    return QWidget::nativeEvent(eventType, message, result);
}

int ScreenPlayerWindow::computeColumnCount() const
{
    if (m_entry.cols > 0) {
        return m_entry.cols;
    }

    const int count = qMax(1, m_entry.videos.size());
    return qCeil(qSqrt(count));
}

int ScreenPlayerWindow::contentRowCount() const
{
    int maxRow = 1;
    for (auto it = m_positions.cbegin(); it != m_positions.cend(); ++it) {
        maxRow = qMax(maxRow, it.value().x());
    }
    return maxRow;
}

int ScreenPlayerWindow::contentColumnCount() const
{
    int maxCol = 1;
    for (auto it = m_positions.cbegin(); it != m_positions.cend(); ++it) {
        maxCol = qMax(maxCol, it.value().y() + 1);
    }
    return maxCol;
}

bool ScreenPlayerWindow::isDragArea(const QPoint &localPos) const
{
    if (!m_titleBar || !m_titleBar->geometry().contains(localPos)) {
        return false;
    }

    QWidget *child = childAt(localPos);
    return child != m_minButton && child != m_fullScreenButton && child != m_closeButton;
}

void ScreenPlayerWindow::updateWindowButtons()
{
    m_fullScreenButton->setText(QString());
    m_fullScreenButton->setIcon(isFullScreen()
        ? iconForPath(":/assets/icons/title_restore.svg")
        : iconForPath(":/assets/icons/title_fullscreen.svg"));
}
