#include "../../include/assistant/SpeechAnimator.h"

#ifdef LORE_WITH_AVATAR
#include "../../include/avatar/AvatarWidget.h"
#endif
#include "inference/InferenceService.h"
#include "voice/TtsManager.h"

#include <QDebug>

namespace {

const QString kSilence = QStringLiteral("sil");

// The blend window. Each viseme holds at full weight for most of its
// duration, then eases into the next viseme over this many
// milliseconds. If a viseme is shorter than twice this, the window
// is halved so the viseme still has a settled centre.
constexpr int kBlendWindowMs = 40;

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
#ifdef LORE_WITH_AVATAR
  m_visemes = chunk.visemes;
  m_visemeIndex = -1;

  if (m_visemes.isEmpty()) {
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
#else
  Q_UNUSED(chunk);
#endif
}

void SpeechAnimator::onSentenceFinished() { finishTimeline(); }

void SpeechAnimator::onTick() {
#ifdef LORE_WITH_AVATAR
  if (!m_playing) {
    m_timer->stop();
    return;
  }

  if (m_inference && !m_inference->isTtsSpeaking()) {
    finishTimeline();
    return;
  }

  const qint64 elapsed = m_clock.elapsed();

  // Find the current viseme by walking the timeline forward. The
  // visemes are ordered and non-overlapping, so a forward walk is
  // correct and O(1) amortised.
  while (m_visemeIndex + 1 < m_visemes.size() &&
         m_visemes.at(m_visemeIndex + 1).startMs <= elapsed) {
    ++m_visemeIndex;
  }

  if (m_visemeIndex < 0) {
    return;
  }

  const Viseme &current = m_visemes.at(m_visemeIndex);

  // Are we inside the blend window at the tail of the current viseme?
  // If so, blend into the next viseme. If not, hold the current shape
  // at full weight.
  const bool hasNext = (m_visemeIndex + 1 < m_visemes.size());

  const int duration = current.endMs - current.startMs;

  const int blendWindow = (duration > 0 && duration < 2 * kBlendWindowMs)
                              ? duration / 2
                              : kBlendWindowMs;

  const int blendStartMs = current.endMs - blendWindow;

  if (hasNext && elapsed >= blendStartMs) {
    const Viseme &next = m_visemes.at(m_visemeIndex + 1);

    float t = 0.0f;
    if (blendWindow > 0) {
      t = static_cast<float>(elapsed - blendStartMs) /
          static_cast<float>(blendWindow);
    }

    t = qBound(0.0f, t, 1.0f);

    applyBlend(current.shape, next.shape, t);
  } else {
    applyShape(current.shape);
  }

  // If we have passed the last viseme's end, the timeline is done.
  if (!m_visemes.isEmpty() && elapsed >= m_visemes.last().endMs) {
    finishTimeline();
  }
#else
  m_timer->stop();
#endif
}

void SpeechAnimator::applyShape(const QString &shape) {
#ifdef LORE_WITH_AVATAR
  if (!m_avatar) {
    return;
  }

  // Track the last applied shape so an identical consecutive shape
  // does not re-trigger the full weight write. The avatar applies it
  // in its own time; this is only an optimisation.
  if (shape == m_lastAppliedShape) {
    return;
  }

  m_lastAppliedShape = shape;

  m_avatar->applyViseme(shape);
#else
  Q_UNUSED(shape);
#endif
}

void SpeechAnimator::applyBlend(const QString &from, const QString &to,
                                float t) {
#ifdef LORE_WITH_AVATAR
  if (!m_avatar) {
    return;
  }

  m_lastAppliedShape.clear();

  m_avatar->applyVisemeBlend(from, to, t);
#else
  Q_UNUSED(from);
  Q_UNUSED(to);
  Q_UNUSED(t);
#endif
}

void SpeechAnimator::finishTimeline() {
  m_playing = false;
  m_timer->stop();
  m_visemes.clear();
  m_visemeIndex = -1;
  m_lastAppliedShape.clear();

#ifdef LORE_WITH_AVATAR
  if (m_avatar) {
    m_avatar->setSpeaking(false);
  }
#endif

  applyShape(kSilence);
}

void SpeechAnimator::reset() { finishTimeline(); }