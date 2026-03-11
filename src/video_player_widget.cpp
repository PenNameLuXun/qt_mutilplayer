#include "video_player_widget.h"

#include "clickable_slider.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QCursor>
#include <QResizeEvent>
#include <QTimer>
#include <QUrl>
#include <QtMultimediaWidgets/QVideoWidget>

namespace
{
constexpr qint64 kSectionToleranceMs = 150;
}

VideoPlayerWidget::VideoPlayerWidget(const VideoEntry &entry, QWidget *parent)
    : QWidget(parent), m_entry(entry)
{
    setupUi();

    m_player = new QMediaPlayer(this);
    m_player->setVideoOutput(m_videoWidget);
    m_player->setActiveAudioTrack(-1);

    connect(m_player, &QMediaPlayer::durationChanged, this, &VideoPlayerWidget::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &VideoPlayerWidget::onPositionChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &VideoPlayerWidget::onMediaStatusChanged);

    const QFileInfo info(entry.path);
    m_player->setSource(QUrl::fromLocalFile(info.absoluteFilePath()));
    m_player->play();
}

void VideoPlayerWidget::stop()
{
    if (m_controlsGuardTimer) {
        m_controlsGuardTimer->stop();
    }
    if (m_controls) {
        m_controls->hide();
    }
    if (m_player) {
        m_player->stop();
    }
}

void VideoPlayerWidget::setFullWindowActive(bool active)
{
    m_fullWindowActive = active;
    updateUi();
}

void VideoPlayerWidget::setFullScreenActive(bool active)
{
    m_fullScreenActive = active;
    updateUi();
}

bool VideoPlayerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_videoWidget || watched == m_controls) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) {
            setControlsVisible(true);
        } else if (event->type() == QEvent::Leave) {
            refreshControlsVisibility();
        }
    }

    if (watched == window() && (event->type() == QEvent::Move || event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateControlsGeometry();
    }

    return QWidget::eventFilter(watched, event);
}

void VideoPlayerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateControlsGeometry();
}

void VideoPlayerWidget::onDurationChanged(qint64 duration)
{
    m_durationMs = duration;
    m_slider->setRange(0, static_cast<int>(duration));
    m_totalLabel->setText(formatTime(duration));

    if (!m_entry.sections.isEmpty()) {
        jumpToSection(0);
    }
}

void VideoPlayerWidget::onPositionChanged(qint64 position)
{
    if (!m_sliderDragging) {
        m_slider->blockSignals(true);
        m_slider->setValue(static_cast<int>(position));
        m_slider->blockSignals(false);
    }

    m_currentLabel->setText(formatTime(position));
    handleSectionLoop(position);
    updateUi();
}

void VideoPlayerWidget::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        if (!m_entry.sections.isEmpty()) {
            jumpToSection(0);
            m_player->play();
        } else {
            seekTo(0);
            m_player->play();
        }
    }
}

void VideoPlayerWidget::togglePlayPause()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
    updateUi();
}

void VideoPlayerWidget::sliderPressed()
{
    m_sliderDragging = true;
}

void VideoPlayerWidget::sliderReleased()
{
    m_sliderDragging = false;
    seekTo(m_slider->value());
}

void VideoPlayerWidget::sliderMoved(int value)
{
    m_currentLabel->setText(formatTime(value));
    if (m_sliderDragging) {
        seekTo(value);
    }
}

void VideoPlayerWidget::toggleFullWindow()
{
    emit requestToggleFullWindow(this, !m_fullWindowActive);
}

void VideoPlayerWidget::toggleFullScreen()
{
    emit requestToggleFullScreen(this, !m_fullScreenActive);
}

void VideoPlayerWidget::refreshControlsVisibility()
{
    setControlsVisible(shouldKeepControlsVisible());
}

void VideoPlayerWidget::setupUi()
{
    setMouseTracking(true);
    setObjectName(QStringLiteral("videoPlayerRoot"));
    setStyleSheet(QStringLiteral(
        "#videoPlayerRoot { background: #111111; }"
        "QWidget#controls { background: rgba(25, 25, 25, 180); border-radius: 8px; }"
        "QPushButton, QLabel { color: white; font-family: 'Microsoft YaHei'; }"
        "QPushButton { background: transparent; border: none; padding: 4px 8px; }"
        "QPushButton:hover { color: #dddddd; }"
        "QSlider::groove:horizontal { height: 4px; background: rgba(255,255,255,70); }"
        "QSlider::sub-page:horizontal { background: white; }"
        "QSlider::handle:horizontal { width: 12px; margin: -5px 0; border-radius: 6px; background: white; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setMouseTracking(true);
    m_videoWidget->installEventFilter(this);
    layout->addWidget(m_videoWidget);

    m_controls = new QWidget(window(), Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_controls->setObjectName(QStringLiteral("controls"));
    m_controls->setMouseTracking(true);
    m_controls->installEventFilter(this);
    m_controls->setAttribute(Qt::WA_ShowWithoutActivating, true);
    auto *controlsLayout = new QHBoxLayout(m_controls);
    controlsLayout->setContentsMargins(12, 6, 12, 6);
    controlsLayout->setSpacing(8);

    m_playButton = new QPushButton(QStringLiteral("Pause"), m_controls);
    m_currentLabel = new QLabel(QStringLiteral("00:00"), m_controls);
    m_slider = new ClickableSlider(Qt::Horizontal, m_controls);
    m_totalLabel = new QLabel(QStringLiteral("00:00"), m_controls);
    m_fullWindowButton = new QPushButton(QStringLiteral("Window"), m_controls);
    m_fullScreenButton = new QPushButton(QStringLiteral("Screen"), m_controls);

    controlsLayout->addWidget(m_playButton);
    controlsLayout->addWidget(m_currentLabel);
    controlsLayout->addWidget(m_slider, 1);
    controlsLayout->addWidget(m_totalLabel);
    controlsLayout->addWidget(m_fullWindowButton);
    controlsLayout->addWidget(m_fullScreenButton);

    connect(m_playButton, &QPushButton::clicked, this, &VideoPlayerWidget::togglePlayPause);
    connect(m_slider, &QSlider::sliderPressed, this, &VideoPlayerWidget::sliderPressed);
    connect(m_slider, &QSlider::sliderReleased, this, &VideoPlayerWidget::sliderReleased);
    connect(m_slider, &QSlider::sliderMoved, this, &VideoPlayerWidget::sliderMoved);
    connect(m_fullWindowButton, &QPushButton::clicked, this, &VideoPlayerWidget::toggleFullWindow);
    connect(m_fullScreenButton, &QPushButton::clicked, this, &VideoPlayerWidget::toggleFullScreen);

    m_controlsGuardTimer = new QTimer(this);
    m_controlsGuardTimer->setInterval(80);
    connect(m_controlsGuardTimer, &QTimer::timeout, this, &VideoPlayerWidget::refreshControlsVisibility);

    window()->installEventFilter(this);

    updateControlsGeometry();
    setControlsVisible(false);
    updateUi();
}

void VideoPlayerWidget::updateUi()
{
    const bool playing = m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
    m_playButton->setText(playing ? QStringLiteral("Pause") : QStringLiteral("Play"));
    m_fullWindowButton->setText(m_fullWindowActive ? QStringLiteral("Restore") : QStringLiteral("Window"));
    m_fullScreenButton->setText(m_fullScreenActive ? QStringLiteral("Exit FS") : QStringLiteral("Screen"));
}

void VideoPlayerWidget::updateControlsGeometry()
{
    if (!m_controls || !m_videoWidget) {
        return;
    }

    const int margin = 16;
    const int controlsHeight = 42;
    const int controlsWidth = qMax(120, m_videoWidget->width() - margin * 2);
    const int controlsY = qMax(margin, m_videoWidget->height() - controlsHeight - margin);
    const QPoint topLeft = m_videoWidget->mapToGlobal(QPoint(margin, controlsY));
    m_controls->setGeometry(topLeft.x(), topLeft.y(), controlsWidth, controlsHeight);
    m_controls->raise();
}

void VideoPlayerWidget::setControlsVisible(bool visible)
{
    if (!m_controls) {
        return;
    }

    m_controls->setVisible(visible);
    if (visible) {
        if (m_controlsGuardTimer && !m_controlsGuardTimer->isActive()) {
            m_controlsGuardTimer->start();
        }
        m_controls->raise();
    } else if (m_controlsGuardTimer) {
        m_controlsGuardTimer->stop();
    }
}

bool VideoPlayerWidget::shouldKeepControlsVisible() const
{
    const QPoint globalPos = QCursor::pos();
    return m_videoWidget->rect().contains(m_videoWidget->mapFromGlobal(globalPos))
        || m_controls->rect().contains(m_controls->mapFromGlobal(globalPos));
}

void VideoPlayerWidget::seekTo(qint64 positionMs)
{
    if (m_player) {
        m_player->setPosition(positionMs);
    }
}

void VideoPlayerWidget::handleSectionLoop(qint64 positionMs)
{
    if (m_ignoreSectionCheck || m_entry.sections.isEmpty()) {
        return;
    }

    if (m_currentSection < 0) {
        jumpToSection(0);
        return;
    }

    const qint64 endMs = sectionEndMs(m_currentSection);
    if (endMs >= 0 && positionMs >= endMs - kSectionToleranceMs) {
        const int nextIndex = (m_currentSection + 1) % m_entry.sections.size();
        jumpToSection(nextIndex);
        m_player->play();
    }
}

qint64 VideoPlayerWidget::sectionEndMs(int index) const
{
    if (index < 0 || index >= m_entry.sections.size()) {
        return -1;
    }

    const PlaySection &section = m_entry.sections.at(index);
    if (section.durationMs < 0) {
        return m_durationMs > 0 ? m_durationMs : -1;
    }
    return section.startMs + section.durationMs;
}

void VideoPlayerWidget::jumpToSection(int index)
{
    if (index < 0 || index >= m_entry.sections.size()) {
        return;
    }

    m_ignoreSectionCheck = true;
    m_currentSection = index;
    seekTo(m_entry.sections.at(index).startMs);
    m_ignoreSectionCheck = false;
}

QString VideoPlayerWidget::formatTime(qint64 ms)
{
    const qint64 totalSeconds = ms / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}
