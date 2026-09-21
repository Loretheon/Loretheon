#ifndef EPISTEME_SPEECHCONTROLLER_H
#define EPISTEME_SPEECHCONTROLLER_H

#include <QObject>
#include <QString>

#include <memory>

class AudioRecorder;
class InferenceService;

// Owns the microphone, drives transcription, and sequences TTS. No UI
// knowledge. Commands use one controller for their audio needs.
//
// Transcription runs on a worker thread because NeMo-Speech's
// transcribe() is synchronous and blocking. TTS is queued one sentence
// at a time because TtsManager rejects overlapping requests.
class SpeechController : public QObject {
  Q_OBJECT

public:
  explicit SpeechController(InferenceService *inference,
                            QObject *parent = nullptr);
  ~SpeechController() override;

  // Microphone capture. beginCapture() starts accumulating; endCapture()
  // stops and begins transcription. The transcribed() signal fires when
  // the result is ready.
  bool beginCapture();
  void endCapture();
  void cancelCapture();

  bool isCapturing() const;
  bool isTranscribing() const;

  // Sentence-by-sentence speaking. speakText() splits the input into
  // sentences and queues them one at a time, advancing on
  // TtsManager::sentenceFinished. stopSpeaking() clears the queue.
  void speakText(const QString &text);
  void stopSpeaking();
  bool isSpeaking() const;

signals:
  // Fired when a captured utterance has been transcribed.
  void transcribed(const QString &text);

  // Fired when capture or transcription fails. The text is a
  // human-readable reason.
  void transcriptionFailed(const QString &error);

  // Fired when speech begins and ends. Useful for the panel to show
  // live state.
  void speakingStarted();
  void speakingFinished();

  // Fired whenever any of the is* state changes.
  void stateChanged();

private:
  void onTranscriptionReady(const QString &text, const QString &error);
  void speakNextSentence();

  InferenceService *m_inference = nullptr;
  std::unique_ptr<AudioRecorder> m_recorder;

  bool m_capturing = false;
  bool m_transcribing = false;

  QStringList m_sentenceQueue;
  QString m_currentSentence;
  bool m_speaking = false;

  quint64 m_captureToken = 0;
};

#endif // EPISTEME_SPEECHCONTROLLER_H