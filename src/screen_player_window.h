#pragma once

#include "app_config.h"

#include <QHash>
#include <QPoint>
#include <QWidget>

class QGridLayout;
class QLabel;
class QPushButton;
class VideoPlayerWidget;

class ScreenPlayerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenPlayerWindow(const ScreenEntry &entry, QWidget *parent = nullptr);
    void placeOnScreen(const QRect &screenGeometry);
    void stop();

signals:
    void closedByUser();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void toggleFullWindow(VideoPlayerWidget *panel, bool enabled);
    void toggleFullScreen(VideoPlayerWidget *panel, bool enabled);
    void toggleLayoutEditMode();
    void closeWindow();

private:
    static ScreenPlayerWindow *windowAtGlobalPosition(const QPoint &globalPos);

    int computeColumnCount() const;
    int contentRowCount() const;
    int contentColumnCount() const;
    void hideTitleBarInFullscreen();
    void updateWindowButtons();
    bool isDragArea(const QPoint &localPos) const;
    int indexOfPanel(VideoPlayerWidget *panel) const;
    int panelIndexAtGlobalPosition(const QPoint &globalPos) const;
    QHash<VideoPlayerWidget *, QRect> panelGlobalGeometries() const;
    void rebuildGrid();
    void rebuildGridAnimated(const QHash<VideoPlayerWidget *, QRect> &oldGlobalGeometries,
                             const QVector<VideoPlayerWidget *> &animatedPanels);
    void setLayoutEditMode(bool enabled);
    void startPanelDrag(VideoPlayerWidget *panel, const QPoint &globalPos);
    void updatePanelDrag(const QPoint &globalPos);
    void finishPanelDrag(const QPoint &globalPos);
    void clearDropTarget();
    void setDropTarget(VideoPlayerWidget *panel);
    void showDropIndicator(int targetIndex);
    void hideDropIndicator();
    void movePanelTo(VideoPlayerWidget *panel, ScreenPlayerWindow *targetWindow, int targetIndex);
    void attachPanel(VideoPlayerWidget *panel, int targetIndex);
    void detachPanel(VideoPlayerWidget *panel);
    void connectPanel(VideoPlayerWidget *panel);
    void installPanelEventFilters(VideoPlayerWidget *panel);
    void removePanelEventFilters(VideoPlayerWidget *panel);

    ScreenEntry m_entry;
    QGridLayout *m_grid = nullptr;
    QWidget *m_titleBar = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_dropHintLabel = nullptr;
    QWidget *m_dropInsertLine = nullptr;
    QPushButton *m_minButton = nullptr;
    QPushButton *m_layoutButton = nullptr;
    QPushButton *m_fullScreenButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    VideoPlayerWidget *m_fullWindowPanel = nullptr;
    VideoPlayerWidget *m_dragPanel = nullptr;
    VideoPlayerWidget *m_dropTargetPanel = nullptr;
    ScreenPlayerWindow *m_dropTargetWindow = nullptr;
    QRect m_normalGeometry;
    QHash<VideoPlayerWidget *, QPoint> m_positions;
    QVector<VideoPlayerWidget *> m_panels;
    bool m_layoutEditMode = false;
    bool m_panelDragging = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
    QPoint m_panelDragStartPos;
    bool m_closeNotified = false;
};
