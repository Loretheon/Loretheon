#pragma once

#include "voice/VisemeMap.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

class AvatarWidget;
class InferenceService;

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
// The clock evaluates the timeline as a function of time. Each viseme
// holds at full weight for most of its duration, then eases into the
// next viseme over a short blend window. That is what keeps a held
// consonant from reading as a sustained pose and makes the mouth move
// through the space between shapes rather than snapping.
class SpeechAnimator : public QObject {
  Q_OBJECT

public:
  explicit SpeechAnimator(InferenceService *inference,
                          AvatarWidget *avatar,
                          QObject *parent = nullptr);
  ~SpeechAnimator() override;

  void setTickIntervalMs(int ms);
  int tickIntervalMs() const;

  bool isPlaying() const { return m_playing; }

public slots:
  void reset();

private slots:
  void onChunkPlaybackStarted(const AudioChunk &chunk);
  void onSentenceFinished();
  void onTick();

private:
  void applyShape(const QString &shape);
  void applyBlend(const QString &from, const QString &to, float t);
  void finishTimeline();

  InferenceService *m_inference = nullptr;
  AvatarWidget *m_avatar = nullptr;

  QTimer *m_timer = nullptr;
  QElapsedTimer m_clock;

  QVector<Viseme> m_visemes;
  int m_visemeIndex = -1;

  bool m_playing = false;
  bool m_warnedAboutEmptyChunk = false;

  // The last shape handed to the avatar. Used to skip redundant
  // writes when the same shape is held across consecutive ticks.
  QString m_lastAppliedShape;

  int m_tickIntervalMs = 16;
};