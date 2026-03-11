#pragma once

#include "app_config.h"

#include <QMediaPlayer>
#include <QWidget>

class QLabel;
class QPushButton;
class QVideoWidget;
class ClickableSlider;
class QTimer;

class VideoPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayerWidget(const VideoEntry &entry, QWidget *parent = nullptr);
    void stop();
    void setFullWindowActive(bool active);
    void setFullScreenActive(bool active);

signals:
    void requestToggleFullWindow(VideoPlayerWidget *panel, bool enabled);
    void requestToggleFullScreen(VideoPlayerWidget *panel, bool enabled);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onDurationChanged(qint64 duration);
    void onPositionChanged(qint64 position);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void togglePlayPause();
    void sliderPressed();
    void sliderReleased();
    void sliderMoved(int value);
    void toggleFullWindow();
    void toggleFullScreen();
    void refreshControlsVisibility();

private:
    void setupUi();
    void updateUi();
    void updateControlsGeometry();
    void setControlsVisible(bool visible);
    bool shouldKeepControlsVisible() const;
    void seekTo(qint64 positionMs);
    void handleSectionLoop(qint64 positionMs);
    qint64 sectionEndMs(int index) const;
    void jumpToSection(int index);
    static QString formatTime(qint64 ms);

    VideoEntry m_entry;
    QVideoWidget *m_videoWidget = nullptr;
    QWidget *m_controls = nullptr;
    QMediaPlayer *m_player = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_fullWindowButton = nullptr;
    QPushButton *m_fullScreenButton = nullptr;
    QLabel *m_currentLabel = nullptr;
    QLabel *m_totalLabel = nullptr;
    ClickableSlider *m_slider = nullptr;
    QTimer *m_controlsGuardTimer = nullptr;
    bool m_sliderDragging = false;
    bool m_ignoreSectionCheck = false;
    int m_currentSection = -1;
    qint64 m_durationMs = 0;
    bool m_fullWindowActive = false;
    bool m_fullScreenActive = false;
};
