#include "../../include/assistant/SpeechAnimator.h"

#include "../../include/avatar/AvatarWidget.h"
#include "inference/InferenceService.h"
#include "voice/TtsManager.h"

#include <QDebug>

namespace {

const QString kSilence = QStringLiteral("sil");

} // namespace

SpeechAnimator::SpeechAnimator(InferenceService *inference,
                               AvatarWidget *avatar, QObject *parent)
    : QObject(parent), m_inference(inference), m_avatar(avatar) {
  m_timer = new QTimer(this);
  m_timer->setInterval(m_tickIntervalMs);
  m_timer->setTimerType(Qt::PreciseTimer);

  connect(m_timer, &QTimer::timeout, this, &SpeechAnimator::onTick);

  if (m_inference) {
    connect(m_inference, &InferenceService::ttsChunkPlaybackStarted, this,
            &SpeechAnimator::onChunkPlaybackStarted);
    connect(m_inference, &InferenceService::ttsSentenceFinished, this,
            &SpeechAnimator::onSentenceFinished);
  }
}

SpeechAnimator::~SpeechAnimator() = default;

void SpeechAnimator::setTickIntervalMs(int ms) {
  m_tickIntervalMs = qBound(8, ms, 100);
  m_timer->setInterval(m_tickIntervalMs);
}

int SpeechAnimator::tickIntervalMs() const { return m_tickIntervalMs; }

void SpeechAnimator::onChunkPlaybackStarted(const AudioChunk &chunk) {
  // A new chunk replaces whatever was playing. Chunks play
  // sequentially, so the previous timeline is already finished; but
  // if the sink restarted early, this is the safe behaviour.
  m_visemes = chunk.visemes;
  m_visemeIndex = -1;

  if (m_visemes.isEmpty()) {
    // Warn once per animator lifetime, not once per chunk, so a
    // configuration problem is visible without flooding the log.
    if (!m_warnedAboutEmptyChunk) {
      qDebug() << "[SpeechAnimator] Chunk carries no visemes; the "
                  "captioned TTS path is probably not in use.";
      m_warnedAboutEmptyChunk = true;
    }
    m_playing = false;
    m_timer->stop();
    applyShape(kSilence);
    return;
  }

  m_playing = true;
  m_clock.start();
  m_timer->start();

  // Apply the first shape immediately, so the mouth moves on the same
  // frame the chunk starts, not one tick later.
  m_visemeIndex = 0;
  applyShape(m_visemes.first().shape);
}

void SpeechAnimator::onSentenceFinished() { finishTimeline(); }

void SpeechAnimator::onTick() {
  if (!m_playing) {
    m_timer->stop();
    return;
  }

  // If the sink stopped for any reason, return to silence.
  if (m_inference && !m_inference->isTtsSpeaking()) {
    finishTimeline();
    return;
  }

  const qint64 elapsed = m_clock.elapsed();

  // Advance the viseme index while the next viseme's start has been
  // passed. A chunk's visemes are ordered and non-overlapping, so a
  // forward walk is correct and O(1) amortised.
  while (m_visemeIndex + 1 < m_visemes.size() &&
         m_visemes.at(m_visemeIndex + 1).startMs <= elapsed) {
    ++m_visemeIndex;
    applyShape(m_visemes.at(m_visemeIndex).shape);
  }

  // If we have passed the last viseme's end, the timeline is done.
  if (!m_visemes.isEmpty() && elapsed >= m_visemes.last().endMs) {
    finishTimeline();
  }
}

void SpeechAnimator::applyShape(const QString &shape) {
  if (!m_avatar) {
    return;
  }

  m_avatar->applyViseme(shape);
}

void SpeechAnimator::finishTimeline() {
  m_playing = false;
  m_timer->stop();
  m_visemes.clear();
  m_visemeIndex = -1;
  applyShape(kSilence);
}

void SpeechAnimator::reset() { finishTimeline(); }