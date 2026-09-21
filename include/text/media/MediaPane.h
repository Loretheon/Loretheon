#ifndef EPISTEME_MEDIAPANE_H
#define EPISTEME_MEDIAPANE_H

#include "MediaKind.h"
#include "MediaSettings.h"

#include <QImage>
#include <QPoint>
#include <QString>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QMovie;
class QPushButton;
class QScrollArea;
class QSlider;
class QStackedWidget;
class QTimer;
class QWheelEvent;

class DiagramDocument;
class DiagramView;

class QAudioOutput;
class QMediaPlayer;
class QVideoWidget;

class MediaPane : public QWidget {
  Q_OBJECT

public:
  explicit MediaPane(QWidget *parent = nullptr);
  ~MediaPane() override;

  bool load(const QString &absolutePath);
  void clear();

  QString currentPath() const { return m_path; }
  MediaKind currentKind() const { return m_kind; }

signals:
  void playbackStateChanged(bool playing);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private slots:
  void onPlayPauseClicked();
  void onSvgBackgroundToggled(bool enabled);
  void onSvgBackgroundClicked();
  void onFitToWindowClicked();
  void onActualSizeClicked();
  void onAnimationTick();

private:
  QWidget *buildMessagePage();
  QWidget *buildImagePage();
  QWidget *buildSvgPage();
  QWidget *buildAudioPage();
  QWidget *buildVideoPage();

  void showMessage(const QString &text);

  void loadRaster(const QString &path);
  void loadSvg(const QString &path);
  void loadAudio(const QString &path);
  void loadVideo(const QString &path);

  // Animated raster (GIF and friends). Decoded frames are kept at
  // native size; zoom is applied at paint time via updateImageDisplay.
  bool loadAnimationFrames(const QString &path);
  void teardownAnimation();
  void applyCurrentFrame();

  void teardownPlayer();

  void updateImageDisplay();
  void fitImageToViewport();
  void showImageAtActualSize();
  void zoomImageAt(const QPoint &viewportPos, double factor);
  void updateZoomLabel();

  void applySvgBackground();
  void syncSvgControlsFromSettings();

  QSize sourceImageSize() const;

  QString m_path;
  MediaKind m_kind = MediaKind::None;

  QStackedWidget *m_stack = nullptr;

  int m_pageMessage = 0;
  int m_pageRaster = 1;
  int m_pageSvg = 2;
  int m_pageAudio = 3;
  int m_pageVideo = 4;

  QLabel *m_messageLabel = nullptr;

  // Raster.
  QScrollArea *m_imageScroll = nullptr;
  QLabel *m_imageLabel = nullptr;
  QImage m_image;
  QPixmap m_currentFramePixmap;

  // Animation state. When m_frames is non-empty the file is animated and
  // m_frameTimer drives the display. The frames are native size.
  QVector<QImage> m_frames;
  QVector<int> m_frameDelays;
  int m_currentFrame = 0;
  QTimer *m_frameTimer = nullptr;
  bool m_animationPlaying = false;

  double m_imageZoom = 0.0;
  double m_referenceZoom = 1.0;

  bool m_panning = false;
  QPoint m_panStart;
  QPoint m_panScrollStart;

  QWidget *m_imageControls = nullptr;
  QPushButton *m_fitButton = nullptr;
  QPushButton *m_actualButton = nullptr;
  QLabel *m_zoomLabel = nullptr;

  // SVG.
  QWidget *m_svgPage = nullptr;
  DiagramView *m_svgView = nullptr;
  DiagramDocument *m_svgDocument = nullptr;
  QCheckBox *m_svgBackgroundCheck = nullptr;
  QPushButton *m_svgBackgroundButton = nullptr;
  QPushButton *m_svgFitButton = nullptr;
  QPushButton *m_svgActualButton = nullptr;
  // Audio.
  QWidget *m_audioPage = nullptr;
  QPushButton *m_audioPlayButton = nullptr;
  QSlider *m_audioSlider = nullptr;
  QLabel *m_audioTimeLabel = nullptr;

  // Video.
  QWidget *m_videoPage = nullptr;
  QVideoWidget *m_videoWidget = nullptr;
  QPushButton *m_videoPlayButton = nullptr;
  QSlider *m_videoSlider = nullptr;
  QLabel *m_videoTimeLabel = nullptr;

  QMediaPlayer *m_player = nullptr;
  QAudioOutput *m_audioOutput = nullptr;

  bool m_sliderDragging = false;
};

#endif // EPISTEME_MEDIAPANE_H