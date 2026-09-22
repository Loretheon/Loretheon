#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

class AvatarWidget;
class InferenceService;

struct AudioChunk;
struct Viseme;

// The viseme clock. Subscribes to InferenceService's TTS chunk signals,
// holds the viseme timeline of the chunk currently playing, and drives
// AvatarWidget::applyViseme(shape) at each boundary.
//
// One chunk at a time. When a new chunk starts, its timeline replaces
// the current one. When the last viseme's end is passed, the mouth is
// returned to "sil" and the clock idles until the next chunk.
//
// ttsChunkReady is deliberately not subscribed to. The same chunk,
// with its viseme vector already populated, arrives on
// ttsChunkPlaybackStarted. One subscription is enough, and it is the
// one that actually marks the start of the clock.
//
// If a chunk carries no visemes (the plain synthesis path rather than
// the captioned path), the clock idles silently for that chunk and
// logs a single warning for the animator's lifetime.
class SpeechAnimator : public QObject {
  Q_OBJECT

public:
  explicit SpeechAnimator(InferenceService *inference,
                          AvatarWidget *avatar,
                          QObject *parent = nullptr);
  ~SpeechAnimator() override;

  // The tick rate of the clock. Default 60 Hz.
  void setTickIntervalMs(int ms);
  int tickIntervalMs() const;

  // True while a viseme timeline is being played.
  bool isPlaying() const { return m_playing; }

public slots:
  // Return the mouth to "sil" and stop the clock. Called on shutdown,
  // when the avatar is hidden, or when speech is aborted.
  void reset();

private slots:
  void onChunkPlaybackStarted(const AudioChunk &chunk);
  void onSentenceFinished();
  void onTick();

private:
  void applyShape(const QString &shape);
  void finishTimeline();

  InferenceService *m_inference = nullptr;
  AvatarWidget *m_avatar = nullptr;

  QTimer *m_timer = nullptr;
  QElapsedTimer m_clock;

  QVector<Viseme> m_visemes;
  int m_visemeIndex = -1;

  bool m_playing = false;
  bool m_warnedAboutEmptyChunk = false;

  int m_tickIntervalMs = 16;
};