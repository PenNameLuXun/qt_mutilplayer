#include "screen_player_window.h"

#include "video_player_widget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QCloseEvent>
#include <QApplication>
#include <QEasingCurve>
#include <QMouseEvent>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QCursor>
#include <QScreen>
#include <QSize>
#include <QStyle>
#include <QtMath>
#include <limits>
#include <windows.h>

namespace
{
constexpr int kTitleBarHeight = 30;
constexpr int kTitleButtonHeight = 24;
constexpr int kResizeBorder = 8;
constexpr int kInsertLineWidth = 6;
constexpr int kLayoutAnimationMs = 180;

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
QVector<ScreenPlayerWindow *> g_windows;

QIcon iconForPath(const char *path)
{
    return QIcon(QString::fromLatin1(path));
}

VideoPlayerWidget *panelForObject(QObject *object)
{
    while (object) {
        if (auto *panel = qobject_cast<VideoPlayerWidget *>(object)) {
            return panel;
        }
        if (auto *widget = qobject_cast<QWidget *>(object)) {
            object = widget->parentWidget();
        } else {
            object = object->parent();
        }
    }
    return nullptr;
}
}

ScreenPlayerWindow::ScreenPlayerWindow(const ScreenEntry &entry, QWidget *parent)
    : QWidget(parent), m_entry(entry)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowTitle(QStringLiteral("Qt Multi Player - Screen %1").arg(entry.screenIndex));
    setWindowIcon(iconForPath(":/assets/icons/app.svg"));
    setMinimumSize(960, 540);
    setMouseTracking(true);

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
    m_layoutButton = new QPushButton(m_titleBar);
    m_fullScreenButton = new QPushButton(m_titleBar);
    m_closeButton = new QPushButton(m_titleBar);
    m_minButton->setObjectName(QStringLiteral("titleMinButton"));
    m_layoutButton->setObjectName(QStringLiteral("titleLayoutButton"));
    m_fullScreenButton->setObjectName(QStringLiteral("titleFullscreenButton"));
    m_closeButton->setObjectName(QStringLiteral("titleCloseButton"));
    m_minButton->setToolTip(QStringLiteral("Minimize"));
    m_layoutButton->setToolTip(QStringLiteral("Toggle Layout Edit Mode"));
    m_fullScreenButton->setToolTip(QStringLiteral("Toggle Fullscreen"));
    m_closeButton->setToolTip(QStringLiteral("Close"));
    m_minButton->setIcon(iconForPath(":/assets/icons/title_min.svg"));
    m_closeButton->setIcon(iconForPath(":/assets/icons/title_close.svg"));
    m_layoutButton->setText(QStringLiteral("Layout"));
    m_minButton->setIconSize(QSize(14, 14));
    m_fullScreenButton->setIconSize(QSize(14, 14));
    m_closeButton->setIconSize(QSize(14, 14));
    m_minButton->setFixedSize(28, kTitleButtonHeight);
    m_layoutButton->setFixedSize(58, kTitleButtonHeight);
    m_fullScreenButton->setFixedSize(34, kTitleButtonHeight);
    m_closeButton->setFixedSize(28, kTitleButtonHeight);

    connect(m_minButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_layoutButton, &QPushButton::clicked, this, &ScreenPlayerWindow::toggleLayoutEditMode);
    connect(m_fullScreenButton, &QPushButton::clicked, this, [this] { toggleFullScreen(nullptr, !isFullScreen()); });
    connect(m_closeButton, &QPushButton::clicked, this, &ScreenPlayerWindow::closeWindow);
    m_titleBar->installEventFilter(this);
    m_titleLabel->installEventFilter(this);

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch(1);
    titleLayout->addWidget(m_minButton);
    titleLayout->addWidget(m_layoutButton);
    titleLayout->addWidget(m_fullScreenButton);
    titleLayout->addWidget(m_closeButton);
    rootLayout->addWidget(m_titleBar, 0, 0, 1, -1);
    rootLayout->setRowStretch(0, 0);

    m_dropHintLabel = new QLabel(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_dropHintLabel->setObjectName(QStringLiteral("dropHintLabel"));
    m_dropHintLabel->setAlignment(Qt::AlignCenter);
    m_dropHintLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_dropHintLabel->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_dropHintLabel->setStyleSheet(QStringLiteral(
        "QLabel#dropHintLabel { color: white; background: rgba(77, 163, 255, 225); "
        "border: 1px solid rgba(255,255,255,180); border-radius: 5px; "
        "padding: 6px 10px; font-size: 13px; font-family: 'Microsoft YaHei'; "
        "font-weight: 600; }"));
    m_dropHintLabel->hide();

    m_dropInsertLine = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_dropInsertLine->setObjectName(QStringLiteral("dropInsertLine"));
    m_dropInsertLine->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_dropInsertLine->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_dropInsertLine->setStyleSheet(QStringLiteral(
        "QWidget#dropInsertLine { background: white; border-radius: 3px; }"));
    m_dropInsertLine->hide();

    const int cols = computeColumnCount();
    for (int i = 0; i < entry.videos.size(); ++i) {
        auto *panel = new VideoPlayerWidget(entry.videos.at(i), this);
        connectPanel(panel);
        installPanelEventFilters(panel);

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
        "QPushButton#titleLayoutButton { min-width: 0px; border-radius: 4px; padding: 0 6px; background: rgba(255,255,255,0.02); font-size: 11px; }"
        "QPushButton#titleMinButton:hover, QPushButton#titleLayoutButton:hover, QPushButton#titleFullscreenButton:hover, QPushButton#titleCloseButton:hover {"
        "  background: rgba(255,255,255,0.14);"
        "}"
        "QPushButton#titleLayoutButton[layoutEdit=\"true\"] { background: rgba(77,163,255,0.32); }"
        "QLabel { font-size: 12px; }"));

    updateWindowButtons();
    g_windows.push_back(this);
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
        const int columnCount = computeColumnCount();
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
        const int columnCount = computeColumnCount();
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
            hideTitleBarInFullscreen();
        }
    } else {
        if (isFullScreen()) {
            showNormal();
            if (m_normalGeometry.isValid()) {
                setGeometry(m_normalGeometry);
            }
            if (m_titleBar) {
                m_titleBar->show();
            }
        }
    }

    for (VideoPlayerWidget *candidate : std::as_const(m_panels)) {
        candidate->setFullScreenActive(isFullScreen());
    }
    updateWindowButtons();
}

void ScreenPlayerWindow::toggleLayoutEditMode()
{
    const bool enabled = !m_layoutEditMode;
    for (ScreenPlayerWindow *window : std::as_const(g_windows)) {
        if (window) {
            window->setLayoutEditMode(enabled);
        }
    }
}

void ScreenPlayerWindow::closeWindow()
{
    close();
}

void ScreenPlayerWindow::closeEvent(QCloseEvent *event)
{
    stop();
    setLayoutEditMode(false);
    g_windows.removeAll(this);
    if (!m_closeNotified) {
        m_closeNotified = true;
        emit closedByUser();
    }
    QWidget::closeEvent(event);
}

bool ScreenPlayerWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (auto *panel = panelForObject(watched)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((m_layoutEditMode && mouseEvent->button() == Qt::LeftButton)
                || mouseEvent->button() == Qt::MiddleButton) {
                startPanelDrag(panel, mouseEvent->globalPosition().toPoint());
                return true;
            }
        }

        if (event->type() == QEvent::MouseMove && m_dragPanel) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            updatePanelDrag(mouseEvent->globalPosition().toPoint());
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease && m_dragPanel) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((m_layoutEditMode && mouseEvent->button() == Qt::LeftButton)
                || mouseEvent->button() == Qt::MiddleButton) {
                finishPanelDrag(mouseEvent->globalPosition().toPoint());
                return true;
            }
        }
    }

    if (m_layoutEditMode) {
        if (watched == m_titleBar || watched == m_titleLabel) {
            return QWidget::eventFilter(watched, event);
        }
    }

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

ScreenPlayerWindow *ScreenPlayerWindow::windowAtGlobalPosition(const QPoint &globalPos)
{
    for (ScreenPlayerWindow *window : std::as_const(g_windows)) {
        if (window && window->isVisible() && window->geometry().contains(globalPos)) {
            return window;
        }
    }
    return nullptr;
}

int ScreenPlayerWindow::indexOfPanel(VideoPlayerWidget *panel) const
{
    return m_panels.indexOf(panel);
}

int ScreenPlayerWindow::panelIndexAtGlobalPosition(const QPoint &globalPos) const
{
    if (!geometry().contains(globalPos)) {
        return -1;
    }

    if (m_panels.isEmpty()) {
        return 0;
    }

    QWidget *child = childAt(mapFromGlobal(globalPos));
    while (child) {
        if (auto *panel = qobject_cast<VideoPlayerWidget *>(child)) {
            return indexOfPanel(panel);
        }
        child = child->parentWidget();
    }

    int nearestIndex = -1;
    qint64 nearestDistance = (std::numeric_limits<qint64>::max)();
    for (int i = 0; i < m_panels.size(); ++i) {
        VideoPlayerWidget *panel = m_panels.at(i);
        const QPoint center = panel->mapToGlobal(panel->rect().center());
        const qint64 dx = center.x() - globalPos.x();
        const qint64 dy = center.y() - globalPos.y();
        const qint64 distance = dx * dx + dy * dy;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestIndex = i;
        }
    }
    return nearestIndex;
}

QHash<VideoPlayerWidget *, QRect> ScreenPlayerWindow::panelGlobalGeometries() const
{
    QHash<VideoPlayerWidget *, QRect> geometries;
    for (VideoPlayerWidget *panel : std::as_const(m_panels)) {
        geometries.insert(panel, QRect(panel->mapToGlobal(QPoint(0, 0)), panel->size()));
    }
    return geometries;
}

void ScreenPlayerWindow::rebuildGrid()
{
    for (VideoPlayerWidget *panel : std::as_const(m_panels)) {
        m_grid->removeWidget(panel);
    }

    m_positions.clear();
    const int cols = computeColumnCount();
    const int rows = qMax(1, qCeil(static_cast<double>(qMax(1, m_panels.size())) / cols));
    for (int row = 1; row <= rows + 1; ++row) {
        m_grid->setRowStretch(row, row <= rows ? 1 : 0);
    }
    for (int col = 0; col < cols + 1; ++col) {
        m_grid->setColumnStretch(col, col < cols ? 1 : 0);
    }

    for (int i = 0; i < m_panels.size(); ++i) {
        VideoPlayerWidget *panel = m_panels.at(i);
        const int row = i / cols + 1;
        const int col = i % cols;
        m_grid->addWidget(panel, row, col);
        m_positions.insert(panel, QPoint(row, col));
        panel->show();
        panel->refreshOverlayParent();
    }
}

void ScreenPlayerWindow::rebuildGridAnimated(
    const QHash<VideoPlayerWidget *, QRect> &oldGlobalGeometries,
    const QVector<VideoPlayerWidget *> &animatedPanels)
{
    rebuildGrid();

    auto *group = new QParallelAnimationGroup(this);
    for (VideoPlayerWidget *panel : animatedPanels) {
        if (!m_panels.contains(panel)) {
            continue;
        }
        if (!oldGlobalGeometries.contains(panel)) {
            continue;
        }

        const QRect endRect = panel->geometry();
        const QRect oldGlobalRect = oldGlobalGeometries.value(panel);
        const QRect startRect(mapFromGlobal(oldGlobalRect.topLeft()), oldGlobalRect.size());
        if (startRect == endRect) {
            continue;
        }

        panel->raise();
        panel->setGeometry(startRect);
        auto *animation = new QPropertyAnimation(panel, "geometry", group);
        animation->setDuration(kLayoutAnimationMs);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->setStartValue(startRect);
        animation->setEndValue(endRect);
        group->addAnimation(animation);
    }

    if (group->animationCount() == 0) {
        group->deleteLater();
        return;
    }

    connect(group, &QParallelAnimationGroup::finished, this, [this] {
        rebuildGrid();
    });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void ScreenPlayerWindow::setLayoutEditMode(bool enabled)
{
    if (m_fullWindowPanel) {
        toggleFullWindow(m_fullWindowPanel, false);
    }

    m_layoutEditMode = enabled;
    if (!enabled) {
        if (m_dragPanel) {
            m_dragPanel->setLayoutDragActive(false);
            m_dragPanel->setCursor(Qt::ArrowCursor);
        }
        clearDropTarget();
        hideDropIndicator();
        m_dragPanel = nullptr;
        m_dropTargetWindow = nullptr;
        m_panelDragging = false;
    }

    if (m_layoutButton) {
        m_layoutButton->setProperty("layoutEdit", enabled);
        m_layoutButton->style()->unpolish(m_layoutButton);
        m_layoutButton->style()->polish(m_layoutButton);
    }

    for (VideoPlayerWidget *panel : std::as_const(m_panels)) {
        panel->setLayoutEditMode(enabled);
    }
}

void ScreenPlayerWindow::startPanelDrag(VideoPlayerWidget *panel, const QPoint &globalPos)
{
    m_dragPanel = panel;
    m_panelDragging = false;
    m_panelDragStartPos = globalPos;
    panel->setCursor(Qt::ClosedHandCursor);
    panel->setLayoutDragActive(true);
}

void ScreenPlayerWindow::updatePanelDrag(const QPoint &globalPos)
{
    if (!m_dragPanel) {
        return;
    }

    if (!m_panelDragging
        && (globalPos - m_panelDragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    m_panelDragging = true;
    ScreenPlayerWindow *targetWindow = windowAtGlobalPosition(globalPos);
    VideoPlayerWidget *targetPanel = nullptr;
    int targetIndex = -1;
    if (targetWindow) {
        targetIndex = targetWindow->panelIndexAtGlobalPosition(globalPos);
        if (targetIndex >= 0 && targetIndex < targetWindow->m_panels.size()) {
            targetPanel = targetWindow->m_panels.at(targetIndex);
        }
    }

    if (targetWindow != m_dropTargetWindow || targetPanel != m_dropTargetPanel) {
        clearDropTarget();
        m_dropTargetWindow = targetWindow;
        setDropTarget(targetPanel);
    }
    if (targetWindow && targetIndex >= 0) {
        targetWindow->showDropIndicator(targetIndex);
    }
}

void ScreenPlayerWindow::finishPanelDrag(const QPoint &globalPos)
{
    VideoPlayerWidget *panel = m_dragPanel;
    if (!panel) {
        return;
    }

    panel->setCursor(m_layoutEditMode ? Qt::OpenHandCursor : Qt::ArrowCursor);
    panel->setLayoutDragActive(false);
    const bool wasDragging = m_panelDragging;
    m_dragPanel = nullptr;
    m_panelDragging = false;

    ScreenPlayerWindow *targetWindow = windowAtGlobalPosition(globalPos);
    const int targetIndex = targetWindow ? targetWindow->panelIndexAtGlobalPosition(globalPos) : -1;
    clearDropTarget();

    if (wasDragging && targetWindow && targetIndex >= 0) {
        movePanelTo(panel, targetWindow, targetIndex);
    }
}

void ScreenPlayerWindow::clearDropTarget()
{
    if (m_dropTargetWindow && m_dropTargetPanel) {
        m_dropTargetPanel->setDropTargetActive(false);
    }
    if (m_dropTargetWindow) {
        m_dropTargetWindow->hideDropIndicator();
    }
    m_dropTargetPanel = nullptr;
    m_dropTargetWindow = nullptr;
}

void ScreenPlayerWindow::setDropTarget(VideoPlayerWidget *panel)
{
    m_dropTargetPanel = panel;
    if (m_dropTargetPanel) {
        m_dropTargetPanel->setDropTargetActive(true);
    }
}

void ScreenPlayerWindow::showDropIndicator(int targetIndex)
{
    if (!m_dropHintLabel || !m_dropInsertLine) {
        return;
    }

    QRect anchorRect;
    if (targetIndex >= 0 && targetIndex < m_panels.size()) {
        anchorRect = m_panels.at(targetIndex)->geometry();
    } else {
        const int top = m_titleBar ? m_titleBar->geometry().bottom() + 8 : 8;
        anchorRect = rect().adjusted(12, top, -12, -12);
    }

    m_dropHintLabel->setText(QStringLiteral("Release to position %1").arg(targetIndex + 1));
    const QSize hintSize = m_dropHintLabel->sizeHint().expandedTo(QSize(150, 30));
    const int x = anchorRect.center().x() - hintSize.width() / 2;
    const int y = anchorRect.top() + 12;
    const QPoint globalTopLeft = mapToGlobal(QPoint(
        qBound(8, x, qMax(8, width() - hintSize.width() - 8)),
        qBound(8, y, qMax(8, height() - hintSize.height() - 8))));
    const QRect boundedRect(
        globalTopLeft.x(),
        globalTopLeft.y(),
        hintSize.width(),
        hintSize.height());
    m_dropHintLabel->setGeometry(boundedRect);
    m_dropHintLabel->show();
    m_dropHintLabel->raise();

    const QPoint lineTopLeft = mapToGlobal(QPoint(
        qBound(8, anchorRect.left() - kInsertLineWidth / 2, qMax(8, width() - kInsertLineWidth - 8)),
        anchorRect.top()));
    const int lineHeight = qMax(30, anchorRect.height());
    m_dropInsertLine->setGeometry(lineTopLeft.x(), lineTopLeft.y(), kInsertLineWidth, lineHeight);
    m_dropInsertLine->show();
    m_dropInsertLine->raise();
}

void ScreenPlayerWindow::hideDropIndicator()
{
    if (m_dropHintLabel) {
        m_dropHintLabel->hide();
    }
    if (m_dropInsertLine) {
        m_dropInsertLine->hide();
    }
}

void ScreenPlayerWindow::movePanelTo(VideoPlayerWidget *panel, ScreenPlayerWindow *targetWindow, int targetIndex)
{
    if (!panel || !targetWindow) {
        return;
    }

    const int sourceIndex = indexOfPanel(panel);
    if (sourceIndex < 0) {
        return;
    }

    QHash<VideoPlayerWidget *, QRect> oldGeometries = panelGlobalGeometries();
    if (targetWindow != this) {
        const QHash<VideoPlayerWidget *, QRect> targetGeometries = targetWindow->panelGlobalGeometries();
        for (auto it = targetGeometries.cbegin(); it != targetGeometries.cend(); ++it) {
            oldGeometries.insert(it.key(), it.value());
        }
    }

    targetIndex = qBound(0, targetIndex, qMax(0, targetWindow->m_panels.size() - 1));
    if (targetWindow == this) {
        if (sourceIndex == targetIndex) {
            return;
        }
        VideoPlayerWidget *targetPanel = m_panels.at(targetIndex);
        m_panels.swapItemsAt(sourceIndex, targetIndex);
        rebuildGridAnimated(oldGeometries, { panel, targetPanel });
        return;
    }

    detachPanel(panel);
    targetWindow->attachPanel(panel, targetIndex);
    rebuildGrid();
    targetWindow->rebuildGrid();
}

void ScreenPlayerWindow::attachPanel(VideoPlayerWidget *panel, int targetIndex)
{
    panel->setParent(this);
    installPanelEventFilters(panel);
    connectPanel(panel);
    targetIndex = qBound(0, targetIndex, m_panels.size());
    m_panels.insert(targetIndex, panel);
    panel->setLayoutEditMode(m_layoutEditMode);
}

void ScreenPlayerWindow::detachPanel(VideoPlayerWidget *panel)
{
    m_grid->removeWidget(panel);
    m_panels.removeAll(panel);
    m_positions.remove(panel);
    removePanelEventFilters(panel);
    disconnect(panel, nullptr, this, nullptr);
}

void ScreenPlayerWindow::connectPanel(VideoPlayerWidget *panel)
{
    connect(panel, &VideoPlayerWidget::requestToggleFullWindow, this, &ScreenPlayerWindow::toggleFullWindow);
    connect(panel, &VideoPlayerWidget::requestToggleFullScreen, this, &ScreenPlayerWindow::toggleFullScreen);
}

void ScreenPlayerWindow::installPanelEventFilters(VideoPlayerWidget *panel)
{
    panel->installEventFilter(this);
    const QList<QWidget *> children = panel->findChildren<QWidget *>();
    for (QWidget *child : children) {
        child->installEventFilter(this);
    }
}

void ScreenPlayerWindow::removePanelEventFilters(VideoPlayerWidget *panel)
{
    panel->removeEventFilter(this);
    const QList<QWidget *> children = panel->findChildren<QWidget *>();
    for (QWidget *child : children) {
        child->removeEventFilter(this);
    }
}

int ScreenPlayerWindow::computeColumnCount() const
{
    if (m_entry.cols > 0) {
        return m_entry.cols;
    }

    const int count = qMax(1, m_panels.isEmpty() ? m_entry.videos.size() : m_panels.size());
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

void ScreenPlayerWindow::hideTitleBarInFullscreen()
{
    if (isFullScreen() && m_titleBar) {
        m_titleBar->hide();
    }
}

bool ScreenPlayerWindow::isDragArea(const QPoint &localPos) const
{
    if (!m_titleBar || !m_titleBar->geometry().contains(localPos)) {
        return false;
    }

    QWidget *child = childAt(localPos);
    return child != m_minButton && child != m_layoutButton && child != m_fullScreenButton && child != m_closeButton;
}

void ScreenPlayerWindow::updateWindowButtons()
{
    m_fullScreenButton->setText(QString());
    m_fullScreenButton->setIcon(isFullScreen()
        ? iconForPath(":/assets/icons/title_restore.svg")
        : iconForPath(":/assets/icons/title_fullscreen.svg"));
}
