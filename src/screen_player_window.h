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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void toggleFullWindow(VideoPlayerWidget *panel, bool enabled);
    void toggleFullScreen(VideoPlayerWidget *panel, bool enabled);
    void closeWindow();

private:
    int computeColumnCount() const;
    int contentRowCount() const;
    int contentColumnCount() const;
    void updateWindowButtons();
    bool isDragArea(const QPoint &localPos) const;

    ScreenEntry m_entry;
    QGridLayout *m_grid = nullptr;
    QWidget *m_titleBar = nullptr;
    QLabel *m_titleLabel = nullptr;
    QPushButton *m_minButton = nullptr;
    QPushButton *m_fullScreenButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    VideoPlayerWidget *m_fullWindowPanel = nullptr;
    QRect m_normalGeometry;
    QHash<VideoPlayerWidget *, QPoint> m_positions;
    QVector<VideoPlayerWidget *> m_panels;
    bool m_dragging = false;
    QPoint m_dragOffset;
};
