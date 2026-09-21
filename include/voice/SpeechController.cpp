#include "../../include/voice/SpeechController.h"

#include "voice/AudioRecorder.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>

namespace {

// Split a block of prose into sentences. Rules:
//   - Terminator is one of . ! ? or an ellipsis.
//   - Trailing quote or bracket marks are kept with the sentence.
//   - Whitespace between sentences is discarded.
//   - A sentence shorter than the minimum is glued to the next one so
//     TTS does not emit a stream of two-word clips.
QStringList splitIntoSentences(const QString &text) {
  constexpr int kMinSentenceLength = 12;

  QStringList raw;

  static const QRegularExpression re(
      QStringLiteral(R"(([^.!?\n]+[.!?…]+["')\]]*\s*|[^.!?\n]+\n))"),
      QRegularExpression::MultilineOption);

  auto it = re.globalMatch(text);
  while (it.hasNext()) {
    raw.append(it.next().captured(0).trimmed());
  }

  // Catch any trailing fragment without a terminator.
  const int matchedLength = [&]() {
    int total = 0;
    for (const QString &s : raw) total += s.length();
    return total;
  }();
  Q_UNUSED(matchedLength);

  QStringList merged;
  QString pending;

  for (const QString &sentence : raw) {
    if (sentence.isEmpty()) {
      continue;
    }

    pending += pending.isEmpty() ? sentence : QStringLiteral(" ") + sentence;

    if (pending.length() >= kMinSentenceLength) {
      merged.append(pending);
      pending.clear();
    }
  }

  if (!pending.isEmpty()) {
    merged.append(pending);
  }

  return merged;
}

} // namespace

SpeechController::SpeechController(InferenceService *inference,
                                   QObject *parent)
    : QObject(parent), m_inference(inference) {

  if (m_inference) {
    connect(m_inference, &InferenceService::ttsSentenceFinished, this,
            &SpeechController::speakNextSentence);
  }


  m_recorder = std::make_unique<AudioRecorder>();
}

SpeechController::~SpeechController() = default;

bool SpeechController::beginCapture() {
  if (m_capturing) {
    return true;
  }

  if (!m_recorder) {
    emit transcriptionFailed(tr("No audio recorder available."));
    return false;
  }

  if (!m_inference || !m_inference->isSttReady()) {
    emit transcriptionFailed(tr("Speech recognition is not ready."));
    return false;
  }

  ++m_captureToken;

  m_recorder->startRecording();
  m_capturing = true;

  emit stateChanged();
  return true;
}

void SpeechController::endCapture() {
  if (!m_capturing || !m_recorder) {
    return;
  }

  m_capturing = false;

  const std::vector<float> pcm = m_recorder->stopRecording();

  if (pcm.empty()) {
    m_transcribing = false;
    emit stateChanged();
    return;
  }

  m_transcribing = true;
  emit stateChanged();

  const quint64 token = m_captureToken;

  // Transcription is synchronous and blocking. Run it on a worker and
  // marshal the result back.
  QtConcurrent::run([this, pcm, token]() {
    QString text;
    QString error;

    if (!m_inference) {
      error = tr("Inference service unavailable.");
    } else {
      text = m_inference->transcribe(pcm);
      if (text.isEmpty()) {
        error = tr("No speech recognised.");
      }
    }

    QMetaObject::invokeMethod(
        this,
        [this, text, error, token]() {
          if (token != m_captureToken) {
            // A newer capture superseded this one.
            return;
          }
          onTranscriptionReady(text, error);
        },
        Qt::QueuedConnection);
  });
}

void SpeechController::cancelCapture() {
  if (!m_recorder) {
    return;
  }

  ++m_captureToken;

  m_capturing = false;
  m_transcribing = false;

  m_recorder->stopRecording();
  emit stateChanged();
}

bool SpeechController::isCapturing() const { return m_capturing; }
bool SpeechController::isTranscribing() const { return m_transcribing; }

void SpeechController::onTranscriptionReady(const QString &text,
                                            const QString &error) {
  m_transcribing = false;
  emit stateChanged();

  if (!error.isEmpty()) {
    emit transcriptionFailed(error);
    return;
  }

  emit transcribed(text);
}

void SpeechController::speakText(const QString &text) {
  if (!m_inference || !m_inference->isTtsReady() ||
      !m_inference->isTtsEnabled()) {
    return;
  }

  stopSpeaking();

  m_sentenceQueue = splitIntoSentences(text);

  if (m_sentenceQueue.isEmpty()) {
    return;
  }

  m_speaking = true;
  emit speakingStarted();
  emit stateChanged();

  speakNextSentence();
}

void SpeechController::speakNextSentence() {
  if (m_sentenceQueue.isEmpty()) {
    m_speaking = false;
    m_currentSentence.clear();
    emit speakingFinished();
    emit stateChanged();
    return;
  }

  m_currentSentence = m_sentenceQueue.takeFirst();

  if (!m_inference) {
    m_speaking = false;
    emit speakingFinished();
    emit stateChanged();
    return;
  }

  m_inference->speak(m_currentSentence);
}

void SpeechController::stopSpeaking() {
  m_sentenceQueue.clear();
  m_currentSentence.clear();

  if (m_inference) {
    m_inference->stopSpeech();
  }

  if (m_speaking) {
    m_speaking = false;
    emit speakingFinished();
    emit stateChanged();
  }
}

bool SpeechController::isSpeaking() const { return m_speaking; }