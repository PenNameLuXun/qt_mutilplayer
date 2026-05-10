#include "video_player_widget.h"

#include "clickable_slider.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QCursor>
#include <QIcon>
#include <QResizeEvent>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QtMultimediaWidgets/QVideoWidget>

namespace
{
constexpr qint64 kSectionToleranceMs = 150;
constexpr int kControlIconSize = 16;
constexpr int kControlButtonPadding = 6;

QIcon iconForPath(const char *path)
{
    return QIcon(QString::fromLatin1(path));
}
}

VideoPlayerWidget::VideoPlayerWidget(const VideoEntry &entry, QWidget *parent)
    : QWidget(parent), m_entry(entry)
{
    setupUi();

    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(0.0f);

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);

    connect(m_player, &QMediaPlayer::durationChanged, this, &VideoPlayerWidget::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &VideoPlayerWidget::onPositionChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &VideoPlayerWidget::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, &VideoPlayerWidget::onPlayerError);

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

void VideoPlayerWidget::setLayoutEditMode(bool active)
{
    m_layoutEditMode = active;
    setProperty("layoutEdit", active);
    style()->unpolish(this);
    style()->polish(this);
    setCursor(active ? Qt::OpenHandCursor : Qt::ArrowCursor);
    if (!active) {
        setLayoutDragActive(false);
        setDropTargetActive(false);
    }
    setControlsVisible(false);
}

void VideoPlayerWidget::setLayoutDragActive(bool active)
{
    m_layoutDragActive = active;
    setProperty("layoutDragging", active);
    style()->unpolish(this);
    style()->polish(this);
    if (m_layoutBadge) {
        if (active) {
            const QString name = QFileInfo(m_entry.path).fileName();
            m_layoutBadge->setText(name.isEmpty()
                ? QStringLiteral("Dragging")
                : QStringLiteral("Dragging: %1").arg(name));
            updateDragMaskGeometry();
            updateLayoutBadgeGeometry();
            if (m_dragMask) {
                m_dragMask->show();
                m_dragMask->raise();
            }
            m_layoutBadge->show();
            m_layoutBadge->raise();
        } else {
            m_layoutBadge->hide();
            if (m_dragMask) {
                m_dragMask->hide();
            }
        }
    }
}

void VideoPlayerWidget::setDropTargetActive(bool active)
{
    m_dropTargetActive = active;
    setProperty("dropTarget", active);
    style()->unpolish(this);
    style()->polish(this);
}

void VideoPlayerWidget::refreshOverlayParent()
{
    QWidget *host = window();
    if (!host) {
        return;
    }

    if (m_overlayHost && m_overlayHost != host) {
        m_overlayHost->removeEventFilter(this);
    }

    m_overlayHost = host;
    if (m_controls && m_controls->parentWidget() != host) {
        m_controls->setParent(host, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        m_controls->setAttribute(Qt::WA_ShowWithoutActivating, true);
        m_controls->installEventFilter(this);
    }
    if (m_volumePopup && m_volumePopup->parentWidget() != host) {
        m_volumePopup->setParent(host, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        m_volumePopup->setAttribute(Qt::WA_ShowWithoutActivating, true);
        m_volumePopup->installEventFilter(this);
    }
    if (m_layoutBadge && m_layoutBadge->parentWidget() != host) {
        m_layoutBadge->setParent(host, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        m_layoutBadge->setAttribute(Qt::WA_ShowWithoutActivating, true);
        m_layoutBadge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    if (m_dragMask && m_dragMask->parentWidget() != host) {
        m_dragMask->setParent(host, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        m_dragMask->setAttribute(Qt::WA_ShowWithoutActivating, true);
        m_dragMask->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    m_overlayHost->installEventFilter(this);
    updateControlsGeometry();
    updateDragMaskGeometry();
    updateLayoutBadgeGeometry();
}

bool VideoPlayerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (m_layoutEditMode) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == m_videoWidget
        || watched == m_controls
        || watched == m_volumePopup
        || watched == m_volumeSlider
        || watched == m_volumeButton) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) {
            setControlsVisible(true);
        } else if (event->type() == QEvent::Leave) {
            refreshControlsVisibility();
        }
    }

    if (watched == m_overlayHost && (event->type() == QEvent::Move || event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateControlsGeometry();
    }

    return QWidget::eventFilter(watched, event);
}

void VideoPlayerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateControlsGeometry();
    updateDragMaskGeometry();
    updateLayoutBadgeGeometry();
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

void VideoPlayerWidget::onVolumeChanged(int value)
{
    if (m_audioOutput) {
        m_audioOutput->setVolume(static_cast<float>(value) / 100.0f);
    }
    updateUi();
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

void VideoPlayerWidget::toggleVolumePopup()
{
    if (!m_volumePopup) {
        return;
    }

    const bool shouldShow = !m_volumePopup->isVisible();
    if (shouldShow) {
        updateVolumePopupGeometry();
        m_volumePopup->show();
        m_volumePopup->raise();
        setControlsVisible(true);
    } else {
        m_volumePopup->hide();
    }
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
        "#videoPlayerRoot[layoutEdit=\"true\"] { border: 2px solid rgba(255,255,255,140); }"
        "#videoPlayerRoot[layoutDragging=\"true\"] { border: 3px solid #ffb020; background: #1f1a10; }"
        "#videoPlayerRoot[dropTarget=\"true\"] { border: 2px solid #4da3ff; }"
        "QWidget#controls { background: rgba(25, 25, 25, 180); border-radius: 8px; }"
        "QWidget#volumePopup { background: rgba(25, 25, 25, 220); border-radius: 8px; }"
        "QPushButton, QLabel { color: white; font-family: 'Microsoft YaHei'; }"
        "QPushButton { background: transparent; border: none; padding: 4px 8px; }"
        "QPushButton#playButton, QPushButton#volumeButton, QPushButton#fullWindowButton, QPushButton#fullScreenButton {"
        "  padding: 0; border-radius: 4px; background: rgba(255,255,255,0.03);"
        "}"
        "QPushButton#playButton:hover, QPushButton#volumeButton:hover, QPushButton#fullWindowButton:hover, QPushButton#fullScreenButton:hover {"
        "  background: rgba(255,255,255,0.16);"
        "}"
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

    m_errorLabel = new QLabel(this);
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #ff6b6b; background: rgba(0,0,0,180); "
        "border-radius: 6px; padding: 8px 12px; font-family: 'Microsoft YaHei'; }"));
    m_errorLabel->hide();

    m_dragMask = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_dragMask->setObjectName(QStringLiteral("dragMask"));
    m_dragMask->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_dragMask->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_dragMask->setAttribute(Qt::WA_TranslucentBackground, true);
    m_dragMask->setWindowOpacity(0.55);
    m_dragMask->setStyleSheet(QStringLiteral(
        "QWidget#dragMask { background: rgba(80, 80, 80, 115); "
        "border: 2px solid rgba(255,255,255,130); }"));
    m_dragMask->hide();

    m_layoutBadge = new QLabel(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_layoutBadge->setObjectName(QStringLiteral("layoutBadge"));
    m_layoutBadge->setAlignment(Qt::AlignCenter);
    m_layoutBadge->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_layoutBadge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_layoutBadge->setStyleSheet(QStringLiteral(
        "QLabel#layoutBadge { color: #111111; background: #ffb020; "
        "border-radius: 4px; padding: 4px 8px; font-size: 12px; "
        "font-family: 'Microsoft YaHei'; font-weight: 600; }"));
    m_layoutBadge->hide();

    m_controls = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_controls->setObjectName(QStringLiteral("controls"));
    m_controls->setMouseTracking(true);
    m_controls->installEventFilter(this);
    m_controls->setAttribute(Qt::WA_ShowWithoutActivating, true);
    auto *controlsLayout = new QHBoxLayout(m_controls);
    //controlsLayout->setContentsMargins(12, 6, 12, 6);
    controlsLayout->setContentsMargins(3,0,0,3);
    controlsLayout->setSpacing(8);

    m_playButton = new QPushButton(m_controls);
    m_playButton->setObjectName(QStringLiteral("playButton"));
    m_playButton->setToolTip(QStringLiteral("Play/Pause"));
    m_playButton->setIconSize(QSize(kControlIconSize, kControlIconSize));
    m_currentLabel = new QLabel(QStringLiteral("00:00"), m_controls);
    m_slider = new ClickableSlider(Qt::Horizontal, m_controls);
    m_totalLabel = new QLabel(QStringLiteral("00:00"), m_controls);
    m_volumeButton = new QPushButton(m_controls);
    m_volumeButton->setObjectName(QStringLiteral("volumeButton"));
    m_volumeButton->setToolTip(QStringLiteral("Volume"));
    m_volumeButton->setIconSize(QSize(kControlIconSize, kControlIconSize));
    m_volumeButton->installEventFilter(this);
    m_fullWindowButton = new QPushButton(m_controls);
    m_fullWindowButton->setObjectName(QStringLiteral("fullWindowButton"));
    m_fullWindowButton->setToolTip(QStringLiteral("Toggle Full Window"));
    m_fullWindowButton->setIconSize(QSize(kControlIconSize, kControlIconSize));
    m_fullScreenButton = new QPushButton(m_controls);
    m_fullScreenButton->setObjectName(QStringLiteral("fullScreenButton"));
    m_fullScreenButton->setToolTip(QStringLiteral("Toggle Full Screen"));
    m_fullScreenButton->setIconSize(QSize(kControlIconSize, kControlIconSize));

    const QSize iconButtonSize(
        kControlIconSize + kControlButtonPadding * 2,
        kControlIconSize + kControlButtonPadding * 2);
    m_playButton->setFixedSize(iconButtonSize);
    m_volumeButton->setFixedSize(iconButtonSize);
    m_fullWindowButton->setFixedSize(iconButtonSize);
    m_fullScreenButton->setFixedSize(iconButtonSize);

    controlsLayout->addWidget(m_playButton);
    controlsLayout->addWidget(m_currentLabel);
    controlsLayout->addWidget(m_slider, 1);
    controlsLayout->addWidget(m_totalLabel);
    controlsLayout->addWidget(m_volumeButton);
    controlsLayout->addWidget(m_fullWindowButton);
    controlsLayout->addWidget(m_fullScreenButton);

    m_volumePopup = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_volumePopup->setObjectName(QStringLiteral("volumePopup"));
    m_volumePopup->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_volumePopup->setMouseTracking(true);
    m_volumePopup->installEventFilter(this);
    auto *volumeLayout = new QVBoxLayout(m_volumePopup);
    volumeLayout->setContentsMargins(10, 10, 10, 10);
    volumeLayout->setSpacing(8);
    auto *volumeLabel = new QLabel(QStringLiteral("Volume"), m_volumePopup);
    m_volumeSlider = new ClickableSlider(Qt::Vertical, m_volumePopup);
    m_volumeSlider->installEventFilter(this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(0);
    m_volumeSlider->setFixedHeight(96);
    volumeLayout->addWidget(volumeLabel, 0, Qt::AlignHCenter);
    volumeLayout->addWidget(m_volumeSlider, 0, Qt::AlignHCenter);
    m_volumePopup->hide();

    connect(m_playButton, &QPushButton::clicked, this, &VideoPlayerWidget::togglePlayPause);
    connect(m_slider, &QSlider::sliderPressed, this, &VideoPlayerWidget::sliderPressed);
    connect(m_slider, &QSlider::sliderReleased, this, &VideoPlayerWidget::sliderReleased);
    connect(m_slider, &QSlider::sliderMoved, this, &VideoPlayerWidget::sliderMoved);
    connect(m_volumeButton, &QPushButton::clicked, this, &VideoPlayerWidget::toggleVolumePopup);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &VideoPlayerWidget::onVolumeChanged);
    connect(m_fullWindowButton, &QPushButton::clicked, this, &VideoPlayerWidget::toggleFullWindow);
    connect(m_fullScreenButton, &QPushButton::clicked, this, &VideoPlayerWidget::toggleFullScreen);

    m_controlsGuardTimer = new QTimer(this);
    m_controlsGuardTimer->setInterval(2500);
    connect(m_controlsGuardTimer, &QTimer::timeout, this, &VideoPlayerWidget::refreshControlsVisibility);

    refreshOverlayParent();

    updateControlsGeometry();
    setControlsVisible(false);
    updateUi();
}

void VideoPlayerWidget::updateUi()
{
    const bool playing = m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
    m_playButton->setText(QString());
    m_playButton->setIcon(playing
        ? iconForPath(":/assets/icons/player_pause.svg")
        : iconForPath(":/assets/icons/player_play.svg"));
    const int volumePercent = m_audioOutput ? qRound(m_audioOutput->volume() * 100.0f) : 0;
    m_volumeButton->setText(QString());
    m_volumeButton->setIcon(volumePercent == 0
        ? iconForPath(":/assets/icons/player_mute.svg")
        : iconForPath(":/assets/icons/player_volume.svg"));
    m_volumeButton->setToolTip(volumePercent == 0
        ? QStringLiteral("Muted")
        : QStringLiteral("Volume %1%").arg(volumePercent));
    m_fullWindowButton->setText(QString());
    m_fullWindowButton->setIcon(m_fullWindowActive
        ? iconForPath(":/assets/icons/player_window_restore.svg")
        : iconForPath(":/assets/icons/player_window.svg"));
    m_fullScreenButton->setText(QString());
    m_fullScreenButton->setIcon(m_fullScreenActive
        ? iconForPath(":/assets/icons/player_screen_restore.svg")
        : iconForPath(":/assets/icons/player_screen.svg"));
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
    updateVolumePopupGeometry();
    updateErrorLabelGeometry();
}

void VideoPlayerWidget::updateVolumePopupGeometry()
{
    if (!m_volumePopup || !m_volumeButton || !m_controls) {
        return;
    }

    const QPoint buttonTopLeft = m_volumeButton->mapToGlobal(QPoint(0, 0));
    const QSize popupSize(72, 140);
    const int x = buttonTopLeft.x() + (m_volumeButton->width() - popupSize.width()) / 2;
    const int y = buttonTopLeft.y() - popupSize.height() - 8;
    m_volumePopup->setGeometry(x, y, popupSize.width(), popupSize.height());
    if (m_volumePopup->isVisible()) {
        m_volumePopup->raise();
    }
}

void VideoPlayerWidget::updateErrorLabelGeometry()
{
    if (!m_errorLabel || !m_videoWidget) {
        return;
    }
    const int w = qMin(m_videoWidget->width() - 32, 400);
    const int h = 56;
    const int x = (m_videoWidget->width() - w) / 2;
    const int y = (m_videoWidget->height() - h) / 2;
    m_errorLabel->setGeometry(x, y, w, h);
    if (m_errorLabel->isVisible()) {
        m_errorLabel->raise();
    }
}

void VideoPlayerWidget::updateLayoutBadgeGeometry()
{
    if (!m_layoutBadge) {
        return;
    }

    const int margin = 10;
    const int maxWidth = qMax(120, width() - margin * 2);
    m_layoutBadge->setMaximumWidth(maxWidth);
    const QSize badgeSize = m_layoutBadge->sizeHint().boundedTo(QSize(maxWidth, 28));
    const QPoint topLeft = mapToGlobal(QPoint(margin, margin));
    m_layoutBadge->setGeometry(topLeft.x(), topLeft.y(), badgeSize.width(), badgeSize.height());
}

void VideoPlayerWidget::updateDragMaskGeometry()
{
    if (!m_dragMask) {
        return;
    }

    const QPoint topLeft = mapToGlobal(QPoint(0, 0));
    m_dragMask->setGeometry(topLeft.x(), topLeft.y(), width(), height());
}

void VideoPlayerWidget::onPlayerError(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error);
    if (m_errorLabel) {
        m_errorLabel->setText(QStringLiteral("播放错误: %1").arg(errorString));
        updateErrorLabelGeometry();
        m_errorLabel->show();
        m_errorLabel->raise();
    }
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
        if (m_volumePopup) {
            m_volumePopup->hide();
        }
    }
}

bool VideoPlayerWidget::shouldKeepControlsVisible() const
{
    const QPoint globalPos = QCursor::pos();
    return m_videoWidget->rect().contains(m_videoWidget->mapFromGlobal(globalPos))
        || m_controls->rect().contains(m_controls->mapFromGlobal(globalPos))
        || (m_volumePopup && m_volumePopup->isVisible() && m_volumePopup->rect().contains(m_volumePopup->mapFromGlobal(globalPos)));
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
