#include "../../include/voice/SpeechController.h"

#include "inference/InferenceService.h"
#include "voice/AudioRecorder.h"

#include <QDebug>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>

namespace {

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

  QStringList merged;
  QString pending;

  for (const QString &sentence : raw) {
    if (sentence.isEmpty()) continue;
    pending += pending.isEmpty() ? sentence : QStringLiteral(" ") + sentence;
    if (pending.length() >= kMinSentenceLength) {
      merged.append(pending);
      pending.clear();
    }
  }

  if (!pending.isEmpty()) merged.append(pending);
  return merged;
}

} // namespace

SpeechController::SpeechController(InferenceService *inference,
                                   QObject *parent)
    : QObject(parent), m_inference(inference) {
  m_recorder = std::make_unique<AudioRecorder>();

  connect(m_recorder.get(), &AudioRecorder::audioChunkReady, this,
          &SpeechController::onAudioChunkReady);

  if (m_inference) {
    connect(m_inference, &InferenceService::ttsSentenceFinished, this,
            &SpeechController::speakNextSentence);
    connect(m_inference, &InferenceService::liveSegment, this,
            &SpeechController::onLiveSegment);
    connect(m_inference, &InferenceService::sttStreamOpened, this,
            &SpeechController::onStreamOpened);
    connect(m_inference, &InferenceService::sttStreamClosed, this,
            &SpeechController::onStreamClosed);
  }
}

SpeechController::~SpeechController() = default;

bool SpeechController::beginCapture(Origin origin) {
  if (m_capturing || m_liveCapturing || m_liveStarting) return false;
  if (!m_recorder) {
    emit transcriptionFailed(tr("No audio recorder available."));
    return false;
  }
  if (!m_inference || !m_inference->isSttReady()) {
    emit transcriptionFailed(tr("Speech recognition is not ready."));
    return false;
  }

  m_captureOrigin = origin;

  ++m_captureToken;
  m_recorder->startRecording();
  m_capturing = true;
  emit stateChanged();
  return true;
}

void SpeechController::endCapture() {
  if (!m_capturing || !m_recorder) return;

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

  QtConcurrent::run([this, pcm, token]() {
    QString text;
    QString error;

    if (!m_inference) {
      error = tr("Inference service unavailable.");
    } else {
      text = m_inference->transcribe(pcm);
      if (text.isEmpty()) error = tr("No speech recognised.");
    }

    QMetaObject::invokeMethod(
        this,
        [this, text, error, token]() {
          if (token != m_captureToken) return;
          onTranscriptionReady(text, error);
        },
        Qt::QueuedConnection);
  });
}

void SpeechController::cancelCapture() {
  if (!m_recorder) return;
  ++m_captureToken;
  m_capturing = false;
  m_transcribing = false;
  m_liveCapturing = false;
  m_liveStarting = false;
  m_recorder->stopRecording();
  if (m_inference) m_inference->stopSttStreaming();
  emit stateChanged();
}

bool SpeechController::isCapturing() const { return m_capturing; }
bool SpeechController::isTranscribing() const { return m_transcribing; }
bool SpeechController::isLiveCapturing() const { return m_liveCapturing; }
bool SpeechController::isLiveStarting() const { return m_liveStarting; }

void SpeechController::startLiveCapture(Origin origin) {
  if (m_liveCapturing || m_liveStarting) return;
  if (m_capturing || m_transcribing) return;
  if (!m_inference || !m_inference->isSttReady()) {
    emit transcriptionFailed(tr("Speech recognition is not ready."));
    return;
  }

  if (!m_inference->startSttStreaming()) {
    emit transcriptionFailed(tr("Could not start live streaming."));
    return;
  }

  m_captureOrigin = origin;

  m_liveStarting = true;
  m_liveCapturing = false;
  emit stateChanged();
}

void SpeechController::stopLiveCapture() {
  if (!m_liveCapturing && !m_liveStarting) return;

  m_liveStarting = false;

  if (m_recorder) m_recorder->stopRecording();

  if (m_inference) m_inference->stopSttStreaming();
}

void SpeechController::onStreamOpened() {
  if (!m_liveStarting) {
    return;
  }

  m_liveStarting = false;
  m_liveCapturing = true;

  if (m_recorder) {
    m_recorder->startRecording();
  }

  emit stateChanged();
}

void SpeechController::onStreamClosed() {
  const bool wasActive = m_liveCapturing || m_liveStarting;

  m_liveCapturing = false;
  m_liveStarting = false;

  if (wasActive) {
    emit stateChanged();
  }
}

void SpeechController::onAudioChunkReady(const std::vector<float> &chunk) {
  if (!m_liveCapturing || !m_inference) return;
  m_inference->feedSttAudio(chunk);
}

void SpeechController::onLiveSegment(const QString &text, bool isFinal) {
  if (text.isEmpty()) return;
  emit liveTranscribed(text, isFinal, m_captureOrigin);
}

void SpeechController::onTranscriptionReady(const QString &text,
                                            const QString &error) {
  m_transcribing = false;
  emit stateChanged();

  if (!error.isEmpty()) {
    emit transcriptionFailed(error);
    return;
  }

  emit transcribed(text, m_captureOrigin);
}

void SpeechController::speakText(const QString &text) {
  if (!m_inference || !m_inference->isTtsReady() ||
      !m_inference->isTtsEnabled()) {
    return;
  }

  stopSpeaking();

  m_sentenceQueue = splitIntoSentences(text);
  if (m_sentenceQueue.isEmpty()) return;

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
  if (m_inference) m_inference->stopSpeech();
  if (m_speaking) {
    m_speaking = false;
    emit speakingFinished();
    emit stateChanged();
  }
}

bool SpeechController::isSpeaking() const { return m_speaking; }