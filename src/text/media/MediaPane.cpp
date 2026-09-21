#include "../../../include/text/media/MediaPane.h"

#include "../../../include/text/diagram/DiagramCanvas.h"
#include "../../../include/text/diagram/DiagramDocument.h"
#include "../../../include/text/diagram/DiagramView.h"

#include <QAudioOutput>
#include <QCheckBox>
#include <QColorDialog>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QWheelEvent>

#include <QUrl>

namespace {

QString formatTime(qint64 ms) {
  if (ms < 0) ms = 0;
  const qint64 totalSeconds = ms / 1000;
  const qint64 minutes = totalSeconds / 60;
  const qint64 seconds = totalSeconds % 60;
  return QStringLiteral("%1:%2")
      .arg(minutes)
      .arg(seconds, 2, 10, QLatin1Char('0'));
}

constexpr int kDefaultFrameDelayMs = 100;

} // namespace

MediaPane::MediaPane(QWidget *parent) : QWidget(parent) {
  m_stack = new QStackedWidget(this);

  m_pageMessage = m_stack->addWidget(buildMessagePage());
  m_pageRaster = m_stack->addWidget(buildImagePage());
  m_pageSvg = m_stack->addWidget(buildSvgPage());
  m_pageAudio = m_stack->addWidget(buildAudioPage());
  m_pageVideo = m_stack->addWidget(buildVideoPage());

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_stack);

  syncSvgControlsFromSettings();
  showMessage(tr("No media loaded."));
}

MediaPane::~MediaPane() {
  teardownAnimation();
  teardownPlayer();
}

QWidget *MediaPane::buildMessagePage() {
  auto *page = new QWidget(this);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(24, 24, 24, 24);

  m_messageLabel = new QLabel(page);
  m_messageLabel->setAlignment(Qt::AlignCenter);
  m_messageLabel->setWordWrap(true);
  m_messageLabel->setObjectName(QStringLiteral("mediaMessageLabel"));

  layout->addStretch();
  layout->addWidget(m_messageLabel);
  layout->addStretch();

  return page;
}

QWidget *MediaPane::buildImagePage() {
  auto *page = new QWidget(this);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_imageScroll = new QScrollArea(page);
  m_imageScroll->setWidgetResizable(false);
  m_imageScroll->setAlignment(Qt::AlignCenter);
  m_imageScroll->setObjectName(QStringLiteral("mediaImageScroll"));
  m_imageScroll->setFrameShape(QFrame::NoFrame);
  m_imageScroll->setFocusPolicy(Qt::NoFocus);
  m_imageScroll->viewport()->installEventFilter(this);
  m_imageScroll->viewport()->setMouseTracking(true);
  m_imageScroll->viewport()->setFocusPolicy(Qt::NoFocus);

  m_imageLabel = new QLabel(m_imageScroll);
  m_imageLabel->setAlignment(Qt::AlignCenter);
  m_imageLabel->setObjectName(QStringLiteral("mediaImageLabel"));
  m_imageLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_imageLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  m_imageScroll->setWidget(m_imageLabel);

  m_imageControls = new QWidget(page);
  m_imageControls->setObjectName(QStringLiteral("mediaImageControls"));
  auto *controlsLayout = new QHBoxLayout(m_imageControls);
  controlsLayout->setContentsMargins(8, 6, 8, 6);
  controlsLayout->setSpacing(8);

  m_fitButton = new QPushButton(tr("Fit"), m_imageControls);
  m_fitButton->setObjectName(QStringLiteral("mediaFitButton"));
  m_fitButton->setToolTip(tr("Fit to window"));

  m_actualButton = new QPushButton(tr("100%"), m_imageControls);
  m_actualButton->setObjectName(QStringLiteral("mediaActualSizeButton"));
  m_actualButton->setToolTip(tr("Actual pixel size"));

  m_zoomLabel = new QLabel(tr("Fit"), m_imageControls);
  m_zoomLabel->setObjectName(QStringLiteral("mediaZoomLabel"));
  m_zoomLabel->setMinimumWidth(72);
  m_zoomLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  controlsLayout->addWidget(m_fitButton);
  controlsLayout->addWidget(m_actualButton);
  controlsLayout->addWidget(m_zoomLabel);
  controlsLayout->addStretch();

  layout->addWidget(m_imageScroll, 1);
  layout->addWidget(m_imageControls);

  connect(m_fitButton, &QPushButton::clicked, this,
          &MediaPane::onFitToWindowClicked);
  connect(m_actualButton, &QPushButton::clicked, this,
          &MediaPane::onActualSizeClicked);

  return page;
}

QWidget *MediaPane::buildSvgPage() {
  m_svgPage = new QWidget(this);
  auto *layout = new QVBoxLayout(m_svgPage);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_svgDocument = new DiagramDocument(this);
  m_svgView = new DiagramView(m_svgPage);
  m_svgView->setDocument(m_svgDocument);

  auto *controls = new QWidget(m_svgPage);
  controls->setObjectName(QStringLiteral("mediaSvgControls"));
  auto *controlsLayout = new QHBoxLayout(controls);
  controlsLayout->setContentsMargins(8, 6, 8, 6);
  controlsLayout->setSpacing(8);

  m_svgFitButton = new QPushButton(tr("Fit"), controls);
  m_svgFitButton->setObjectName(QStringLiteral("mediaSvgFitButton"));
  m_svgFitButton->setToolTip(tr("Fit to window"));

  m_svgActualButton = new QPushButton(tr("100%"), controls);
  m_svgActualButton->setObjectName(QStringLiteral("mediaSvgActualButton"));
  m_svgActualButton->setToolTip(tr("Actual pixel size"));

  m_svgBackgroundCheck = new QCheckBox(tr("Custom background"), controls);
  m_svgBackgroundCheck->setObjectName(
      QStringLiteral("mediaSvgBackgroundCheck"));

  m_svgBackgroundButton = new QPushButton(tr("Colour…"), controls);
  m_svgBackgroundButton->setObjectName(
      QStringLiteral("mediaSvgBackgroundButton"));

  controlsLayout->addWidget(m_svgFitButton);
  controlsLayout->addWidget(m_svgActualButton);
  controlsLayout->addSpacing(12);
  controlsLayout->addWidget(m_svgBackgroundCheck);
  controlsLayout->addWidget(m_svgBackgroundButton);
  controlsLayout->addStretch();

  layout->addWidget(m_svgView, 1);
  layout->addWidget(controls);

  connect(m_svgFitButton, &QPushButton::clicked, this, [this]() {
    if (m_svgView) m_svgView->zoomFit();
  });
  connect(m_svgActualButton, &QPushButton::clicked, this, [this]() {
    if (m_svgView) m_svgView->setZoom(1.0);
  });
  connect(m_svgBackgroundCheck, &QCheckBox::toggled, this,
          &MediaPane::onSvgBackgroundToggled);
  connect(m_svgBackgroundButton, &QPushButton::clicked, this,
          &MediaPane::onSvgBackgroundClicked);

  return m_svgPage;
}

QWidget *MediaPane::buildAudioPage() {
  m_audioPage = new QWidget(this);
  auto *layout = new QVBoxLayout(m_audioPage);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->setSpacing(12);

  auto *titleLabel = new QLabel(tr("Audio"), m_audioPage);
  titleLabel->setAlignment(Qt::AlignCenter);
  titleLabel->setObjectName(QStringLiteral("mediaAudioTitle"));

  m_audioTimeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), m_audioPage);
  m_audioTimeLabel->setAlignment(Qt::AlignCenter);
  m_audioTimeLabel->setObjectName(QStringLiteral("mediaAudioTime"));

  m_audioSlider = new QSlider(Qt::Horizontal, m_audioPage);
  m_audioSlider->setRange(0, 0);
  m_audioSlider->setObjectName(QStringLiteral("mediaAudioSlider"));

  m_audioPlayButton = new QPushButton(tr("Play"), m_audioPage);
  m_audioPlayButton->setObjectName(QStringLiteral("mediaAudioPlayButton"));

  auto *controls = new QHBoxLayout();
  controls->addStretch();
  controls->addWidget(m_audioPlayButton);
  controls->addStretch();

  layout->addStretch();
  layout->addWidget(titleLabel);
  layout->addWidget(m_audioTimeLabel);
  layout->addWidget(m_audioSlider);
  layout->addLayout(controls);
  layout->addStretch();

  connect(m_audioPlayButton, &QPushButton::clicked, this,
          &MediaPane::onPlayPauseClicked);
  connect(m_audioSlider, &QSlider::sliderPressed, this,
          [this]() { m_sliderDragging = true; });
  connect(m_audioSlider, &QSlider::sliderReleased, this, [this]() {
    m_sliderDragging = false;
    if (m_player) m_player->setPosition(m_audioSlider->value());
  });

  return m_audioPage;
}

QWidget *MediaPane::buildVideoPage() {
  m_videoPage = new QWidget(this);
  auto *layout = new QVBoxLayout(m_videoPage);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_videoWidget = new QVideoWidget(m_videoPage);
  m_videoWidget->setObjectName(QStringLiteral("mediaVideoWidget"));
  m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto *controls = new QWidget(m_videoPage);
  controls->setObjectName(QStringLiteral("mediaVideoControls"));
  auto *controlsLayout = new QHBoxLayout(controls);
  controlsLayout->setContentsMargins(8, 6, 8, 6);
  controlsLayout->setSpacing(8);

  m_videoPlayButton = new QPushButton(tr("Play"), controls);
  m_videoPlayButton->setObjectName(QStringLiteral("mediaVideoPlayButton"));

  m_videoSlider = new QSlider(Qt::Horizontal, controls);
  m_videoSlider->setRange(0, 0);
  m_videoSlider->setObjectName(QStringLiteral("mediaVideoSlider"));

  m_videoTimeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), controls);
  m_videoTimeLabel->setObjectName(QStringLiteral("mediaVideoTime"));

  controlsLayout->addWidget(m_videoPlayButton);
  controlsLayout->addWidget(m_videoSlider, 1);
  controlsLayout->addWidget(m_videoTimeLabel);

  layout->addWidget(m_videoWidget, 1);
  layout->addWidget(controls);

  connect(m_videoPlayButton, &QPushButton::clicked, this,
          &MediaPane::onPlayPauseClicked);
  connect(m_videoSlider, &QSlider::sliderPressed, this,
          [this]() { m_sliderDragging = true; });
  connect(m_videoSlider, &QSlider::sliderReleased, this, [this]() {
    m_sliderDragging = false;
    if (m_player) m_player->setPosition(m_videoSlider->value());
  });

  return m_videoPage;
}

void MediaPane::showMessage(const QString &text) {
  m_messageLabel->setText(text);
  m_stack->setCurrentIndex(m_pageMessage);
}

void MediaPane::teardownPlayer() {
  if (m_player) {
    m_player->stop();
    m_player->setSource(QUrl());
    m_player->setVideoOutput(nullptr);
    m_player->deleteLater();
    m_player = nullptr;
  }
  if (m_audioOutput) {
    m_audioOutput->deleteLater();
    m_audioOutput = nullptr;
  }
  m_sliderDragging = false;
}

void MediaPane::teardownAnimation() {
  if (m_frameTimer) {
    m_frameTimer->stop();
    m_frameTimer->deleteLater();
    m_frameTimer = nullptr;
  }
  m_frames.clear();
  m_frameDelays.clear();
  m_currentFrame = 0;
  m_animationPlaying = false;
  m_currentFramePixmap = QPixmap();
}

void MediaPane::syncSvgControlsFromSettings() {
  auto &settings = MediaSettings::instance();

  if (m_svgBackgroundCheck) {
    QSignalBlocker blocker(m_svgBackgroundCheck);
    m_svgBackgroundCheck->setChecked(settings.svgBackgroundEnabled());
  }

  applySvgBackground();
}

bool MediaPane::eventFilter(QObject *watched, QEvent *event) {
  const bool isViewport =
      m_imageScroll && watched == m_imageScroll->viewport();

  if (!isViewport) {
    return QWidget::eventFilter(watched, event);
  }

  if (event->type() == QEvent::Resize) {
    if (m_kind == MediaKind::Raster && m_imageZoom <= 0.0) {
      updateImageDisplay();
    }
    return QWidget::eventFilter(watched, event);
  }

  if (m_kind != MediaKind::Raster) {
    return QWidget::eventFilter(watched, event);
  }

  if (event->type() == QEvent::Wheel) {
    auto *wheel = static_cast<QWheelEvent *>(event);

    const int delta = wheel->angleDelta().y();
    if (delta == 0) {
      return false;
    }

    if (wheel->modifiers() & Qt::ControlModifier) {
      const double factor = delta > 0 ? 1.15 : (1.0 / 1.15);
      const QPoint viewportPos = wheel->position().toPoint();
      zoomImageAt(viewportPos, factor);
      return true;
    }

    QScrollBar *vBar = m_imageScroll->verticalScrollBar();
    if (vBar && vBar->maximum() > vBar->minimum()) {
      const int step = vBar->singleStep() > 0
                           ? vBar->singleStep() * 3
                           : 60;
      vBar->setValue(vBar->value() - delta / 120 * step);
    }
    return true;
  }

  if (event->type() == QEvent::MouseButtonPress) {
    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() == Qt::RightButton) {
      m_panning = true;
      m_panStart = mouse->pos();
      m_panScrollStart = QPoint(
          m_imageScroll->horizontalScrollBar()->value(),
          m_imageScroll->verticalScrollBar()->value());
      m_imageScroll->viewport()->setCursor(Qt::ClosedHandCursor);
      return true;
    }
  } else if (event->type() == QEvent::MouseMove) {
    auto *mouse = static_cast<QMouseEvent *>(event);
    if (m_panning) {
      const QPoint delta = mouse->pos() - m_panStart;
      m_imageScroll->horizontalScrollBar()->setValue(
          m_panScrollStart.x() - delta.x());
      m_imageScroll->verticalScrollBar()->setValue(
          m_panScrollStart.y() - delta.y());
      return true;
    }
  } else if (event->type() == QEvent::MouseButtonRelease) {
    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() == Qt::RightButton && m_panning) {
      m_panning = false;
      m_imageScroll->viewport()->unsetCursor();
      return true;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void MediaPane::wheelEvent(QWheelEvent *event) {
  QWidget::wheelEvent(event);
}

QSize MediaPane::sourceImageSize() const {
  if (!m_frames.isEmpty()) {
    return m_frames.first().size();
  }
  return m_image.size();
}

void MediaPane::zoomImageAt(const QPoint &viewportPos, double factor) {
  if (!m_imageLabel) {
    return;
  }

  const QSize sourceSize = sourceImageSize();
  if (!sourceSize.isValid() || sourceSize.isEmpty()) {
    return;
  }

  double currentZoom = m_imageZoom;
  if (currentZoom <= 0.0) {
    const QSize viewport = m_imageScroll->viewport()->size();
    if (viewport.isValid() && !viewport.isEmpty() && sourceSize.width() > 0 &&
        sourceSize.height() > 0) {
      currentZoom =
          qMin(static_cast<double>(viewport.width()) / sourceSize.width(),
               static_cast<double>(viewport.height()) / sourceSize.height());
    } else {
      currentZoom = 1.0;
    }
  }

  const double nextZoom = qBound(0.05, currentZoom * factor, 20.0);

  QScrollBar *hBar = m_imageScroll->horizontalScrollBar();
  QScrollBar *vBar = m_imageScroll->verticalScrollBar();

  const int oldScrollX = hBar ? hBar->value() : 0;
  const int oldScrollY = vBar ? vBar->value() : 0;

  const int oldWidth =
      qMax(1, static_cast<int>(sourceSize.width() * currentZoom));
  const int oldHeight =
      qMax(1, static_cast<int>(sourceSize.height() * currentZoom));

  const QSize viewport = m_imageScroll->viewport()->size();
  const int oldOffsetX =
      oldWidth < viewport.width() ? (viewport.width() - oldWidth) / 2 : 0;
  const int oldOffsetY =
      oldHeight < viewport.height() ? (viewport.height() - oldHeight) / 2 : 0;

  const double sourceX =
      (oldScrollX + viewportPos.x() - oldOffsetX) / currentZoom;
  const double sourceY =
      (oldScrollY + viewportPos.y() - oldOffsetY) / currentZoom;

  m_imageZoom = nextZoom;
  updateImageDisplay();

  const int newWidth =
      qMax(1, static_cast<int>(sourceSize.width() * nextZoom));
  const int newHeight =
      qMax(1, static_cast<int>(sourceSize.height() * nextZoom));

  const int newOffsetX =
      newWidth < viewport.width() ? (viewport.width() - newWidth) / 2 : 0;
  const int newOffsetY =
      newHeight < viewport.height() ? (viewport.height() - newHeight) / 2 : 0;

  const int targetScrollX =
      static_cast<int>(sourceX * nextZoom) + newOffsetX - viewportPos.x();
  const int targetScrollY =
      static_cast<int>(sourceY * nextZoom) + newOffsetY - viewportPos.y();

  if (hBar) hBar->setValue(targetScrollX);
  if (vBar) vBar->setValue(targetScrollY);

  updateZoomLabel();
}

void MediaPane::updateZoomLabel() {
  if (!m_zoomLabel) {
    return;
  }

  if (m_imageZoom <= 0.0) {
    m_zoomLabel->setText(tr("Fit"));
    return;
  }

  const int percent = qRound(m_imageZoom * 100.0);
  m_zoomLabel->setText(QStringLiteral("%1%").arg(percent));
}

void MediaPane::fitImageToViewport() {
  m_imageZoom = 0.0;
  updateImageDisplay();

  if (m_zoomLabel) {
    m_zoomLabel->setText(tr("Fit"));
  }
}

void MediaPane::showImageAtActualSize() {
  // 1.0 means one source pixel per screen pixel. The reference for the
  // zoom label is always source size, so clicking this sets the zoom
  // factor to 1.0 and the label reads 100%.
  m_imageZoom = 1.0;
  m_referenceZoom = 1.0;
  updateImageDisplay();
  updateZoomLabel();
}

void MediaPane::onFitToWindowClicked() { fitImageToViewport(); }

void MediaPane::onActualSizeClicked() { showImageAtActualSize(); }

void MediaPane::onAnimationTick() {
  if (m_frames.isEmpty() || !m_animationPlaying) {
    return;
  }

  m_currentFrame = (m_currentFrame + 1) % m_frames.size();
  applyCurrentFrame();

  if (m_frameTimer) {
    const int delay = m_currentFrame < m_frameDelays.size()
                          ? m_frameDelays.at(m_currentFrame)
                          : kDefaultFrameDelayMs;
    m_frameTimer->start(qMax(10, delay));
  }
}

void MediaPane::applyCurrentFrame() {
  if (m_frames.isEmpty() || !m_imageLabel) {
    return;
  }

  const int index = qBound(0, m_currentFrame, m_frames.size() - 1);
  m_currentFramePixmap = QPixmap::fromImage(m_frames.at(index));

  updateImageDisplay();
}

void MediaPane::applySvgBackground() {
  if (!m_svgView) {
    return;
  }

  auto &settings = MediaSettings::instance();
  const bool enabled = settings.svgBackgroundEnabled();
  const QColor color = settings.svgBackground();

  const QList<DiagramCanvas *> canvases =
      m_svgView->findChildren<DiagramCanvas *>();
  for (DiagramCanvas *canvas : canvases) {
    if (!canvas) continue;
    QPalette pal = canvas->palette();
    pal.setColor(QPalette::Window, enabled ? color : Qt::transparent);
    canvas->setPalette(pal);
    canvas->setAutoFillBackground(enabled);
    canvas->update();
  }

  QPalette viewPal = m_svgView->palette();
  viewPal.setColor(QPalette::Window, enabled ? color : Qt::transparent);
  m_svgView->setPalette(viewPal);
  m_svgView->setAutoFillBackground(enabled);

  if (m_svgDocument) {
    m_svgView->update();
  }
}

void MediaPane::onSvgBackgroundToggled(bool enabled) {
  MediaSettings::instance().setSvgBackgroundEnabled(enabled);
  applySvgBackground();
}

void MediaPane::onSvgBackgroundClicked() {
  auto &settings = MediaSettings::instance();

  const QColor chosen = QColorDialog::getColor(
      settings.svgBackground(), this, tr("SVG Background"),
      QColorDialog::ShowAlphaChannel);

  if (!chosen.isValid()) {
    return;
  }

  settings.setSvgBackground(chosen);

  if (!settings.svgBackgroundEnabled()) {
    settings.setSvgBackgroundEnabled(true);
    if (m_svgBackgroundCheck) {
      QSignalBlocker blocker(m_svgBackgroundCheck);
      m_svgBackgroundCheck->setChecked(true);
    }
  }

  applySvgBackground();
}

bool MediaPane::load(const QString &absolutePath) {
  clear();

  if (absolutePath.isEmpty()) {
    showMessage(tr("No file selected."));
    return false;
  }

  const QFileInfo info(absolutePath);
  if (!info.exists() || !info.isFile()) {
    showMessage(tr("File not found: %1").arg(absolutePath));
    return false;
  }

  m_path = info.absoluteFilePath();
  m_kind = MediaKinds::kindForPath(m_path);

  switch (m_kind) {
  case MediaKind::Raster:
    loadRaster(m_path);
    return true;
  case MediaKind::Vector:
    loadSvg(m_path);
    return true;
  case MediaKind::Audio:
    loadAudio(m_path);
    return true;
  case MediaKind::Video:
    loadVideo(m_path);
    return true;
  case MediaKind::None:
    break;
  }

  showMessage(tr("Unsupported media type: %1").arg(info.fileName()));
  return false;
}

bool MediaPane::loadAnimationFrames(const QString &path) {
  teardownAnimation();

  QImageReader reader(path);
  reader.setAutoTransform(true);

  if (!reader.canRead() || !reader.supportsAnimation()) {
    return false;
  }

  const int count = reader.imageCount();
  if (count <= 1) {
    return false;
  }

  m_frames.reserve(count);
  m_frameDelays.reserve(count);

  for (int i = 0; i < count; ++i) {
    const QImage frame = reader.read();
    if (frame.isNull()) {
      break;
    }
    m_frames.append(frame);
    m_frameDelays.append(qMax(10, reader.nextImageDelay()));
  }

  if (m_frames.size() <= 1) {
    teardownAnimation();
    return false;
  }

  m_currentFrame = 0;
  m_currentFramePixmap = QPixmap::fromImage(m_frames.first());

  m_frameTimer = new QTimer(this);
  m_frameTimer->setSingleShot(true);
  connect(m_frameTimer, &QTimer::timeout, this, &MediaPane::onAnimationTick);

  m_animationPlaying = MediaSettings::instance().autoplayGif();
  if (m_animationPlaying) {
    const int delay = m_frameDelays.value(0, kDefaultFrameDelayMs);
    m_frameTimer->start(qMax(10, delay));
  }

  return true;
}

void MediaPane::loadRaster(const QString &path) {
  teardownAnimation();
  teardownPlayer();

  m_image = QImage();
  m_imageZoom = 0.0;
  m_referenceZoom = 1.0;
  m_panning = false;

  if (loadAnimationFrames(path)) {
    m_stack->setCurrentIndex(m_pageRaster);
    fitImageToViewport();
    return;
  }

  QImageReader reader(path);
  reader.setAutoTransform(true);

  if (!reader.canRead()) {
    showMessage(tr("Cannot read image: %1\n%2")
                    .arg(QFileInfo(path).fileName(), reader.errorString()));
    return;
  }

  const QImage image = reader.read();
  if (image.isNull()) {
    showMessage(tr("Failed to decode image: %1")
                    .arg(QFileInfo(path).fileName()));
    return;
  }

  m_image = image;
  m_currentFramePixmap = QPixmap::fromImage(image);

  m_stack->setCurrentIndex(m_pageRaster);
  fitImageToViewport();
}

void MediaPane::loadSvg(const QString &path) {
  if (!m_svgDocument || !m_svgView) {
    showMessage(tr("SVG viewer unavailable."));
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    showMessage(tr("Cannot read: %1").arg(QFileInfo(path).fileName()));
    return;
  }

  const QString svg = QString::fromUtf8(file.readAll());
  file.close();

  if (svg.isEmpty()) {
    showMessage(tr("Empty SVG: %1").arg(QFileInfo(path).fileName()));
    return;
  }

  m_svgDocument->setSvg(svg);
  m_svgView->zoomFit();

  applySvgBackground();
  m_stack->setCurrentIndex(m_pageSvg);
}

void MediaPane::loadAudio(const QString &path) {
  m_player = new QMediaPlayer(this);
  m_audioOutput = new QAudioOutput(this);
  m_player->setAudioOutput(m_audioOutput);

  connect(m_player, &QMediaPlayer::positionChanged, this,
          [this](qint64 position) {
            if (m_sliderDragging) return;
            if (m_audioSlider)
              m_audioSlider->setValue(static_cast<int>(position));
            if (m_audioTimeLabel) {
              m_audioTimeLabel->setText(
                  QStringLiteral("%1 / %2")
                      .arg(formatTime(position),
                           formatTime(m_player ? m_player->duration() : 0)));
            }
          });
  connect(m_player, &QMediaPlayer::durationChanged, this,
          [this](qint64 duration) {
            const int capped =
                duration > INT_MAX ? INT_MAX : static_cast<int>(duration);
            if (m_audioSlider) m_audioSlider->setRange(0, capped);
          });
  connect(m_player, &QMediaPlayer::errorOccurred, this,
          [this](QMediaPlayer::Error, const QString &errorString) {
            showMessage(tr("Playback error: %1").arg(errorString));
          });
  connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this]() {
    if (!m_player) return;
    const bool playing =
        m_player->playbackState() == QMediaPlayer::PlayingState;
    if (m_audioPlayButton) {
      m_audioPlayButton->setText(playing ? tr("Pause") : tr("Play"));
    }
    emit playbackStateChanged(playing);
  });

  m_audioPlayButton->setText(tr("Play"));
  m_audioSlider->setRange(0, 0);
  m_audioTimeLabel->setText(QStringLiteral("0:00 / 0:00"));

  m_player->setSource(QUrl::fromLocalFile(path));
  m_stack->setCurrentIndex(m_pageAudio);
}

void MediaPane::loadVideo(const QString &path) {
  m_player = new QMediaPlayer(this);
  m_audioOutput = new QAudioOutput(this);
  m_player->setAudioOutput(m_audioOutput);

  m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);

  connect(m_player, &QMediaPlayer::positionChanged, this,
          [this](qint64 position) {
            if (m_sliderDragging) return;
            if (m_videoSlider)
              m_videoSlider->setValue(static_cast<int>(position));
            if (m_videoTimeLabel) {
              m_videoTimeLabel->setText(
                  QStringLiteral("%1 / %2")
                      .arg(formatTime(position),
                           formatTime(m_player ? m_player->duration() : 0)));
            }
          });
  connect(m_player, &QMediaPlayer::durationChanged, this,
          [this](qint64 duration) {
            const int capped =
                duration > INT_MAX ? INT_MAX : static_cast<int>(duration);
            if (m_videoSlider) m_videoSlider->setRange(0, capped);
          });
  connect(m_player, &QMediaPlayer::errorOccurred, this,
          [this](QMediaPlayer::Error, const QString &errorString) {
            showMessage(tr("Playback error: %1").arg(errorString));
          });
  connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this]() {
    if (!m_player) return;
    const bool playing =
        m_player->playbackState() == QMediaPlayer::PlayingState;
    if (m_videoPlayButton) {
      m_videoPlayButton->setText(playing ? tr("Pause") : tr("Play"));
    }
    emit playbackStateChanged(playing);
  });

  m_videoPlayButton->setText(tr("Play"));
  m_videoSlider->setRange(0, 0);
  m_videoTimeLabel->setText(QStringLiteral("0:00 / 0:00"));

  m_player->setVideoOutput(m_videoWidget);
  m_player->setSource(QUrl::fromLocalFile(path));
  m_stack->setCurrentIndex(m_pageVideo);
}

void MediaPane::clear() {
  teardownAnimation();
  teardownPlayer();

  m_path.clear();
  m_kind = MediaKind::None;
  m_image = QImage();
  m_imageZoom = 0.0;
  m_referenceZoom = 1.0;
  m_panning = false;

  if (m_imageLabel) {
    m_imageLabel->clear();
    m_imageLabel->setPixmap(QPixmap());
  }
  if (m_svgDocument) {
    m_svgDocument->clear();
  }
  if (m_audioSlider) {
    m_audioSlider->setRange(0, 0);
  }
  if (m_videoSlider) {
    m_videoSlider->setRange(0, 0);
  }
  if (m_zoomLabel) {
    m_zoomLabel->setText(tr("Fit"));
  }

  showMessage(tr("No media loaded."));
}

void MediaPane::updateImageDisplay() {
  if (!m_imageLabel || !m_imageScroll) {
    return;
  }

  const QSize sourceSize = sourceImageSize();
  if (!sourceSize.isValid() || sourceSize.isEmpty()) {
    return;
  }

  QSize target = sourceSize;

  if (m_imageZoom <= 0.0) {
    const QSize viewport = m_imageScroll->viewport()->size();
    if (viewport.isValid() && !viewport.isEmpty()) {
      target = sourceSize.scaled(viewport, Qt::KeepAspectRatio);
    }
  } else {
    target = QSize(qMax(1, static_cast<int>(sourceSize.width() * m_imageZoom)),
                   qMax(1, static_cast<int>(sourceSize.height() * m_imageZoom)));
  }

  if (target == sourceSize && !m_currentFramePixmap.isNull()) {
    m_imageLabel->setPixmap(m_currentFramePixmap);
  } else if (!m_currentFramePixmap.isNull()) {
    const QPixmap scaled = m_currentFramePixmap.scaled(
        target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(scaled);
  }

  m_imageLabel->setFixedSize(target);
}

void MediaPane::onPlayPauseClicked() {
  if (!m_player) {
    return;
  }

  if (m_player->playbackState() == QMediaPlayer::PlayingState) {
    m_player->pause();
  } else {
    m_player->play();
  }
}