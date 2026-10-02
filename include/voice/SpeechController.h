#ifndef EPISTEME_SPEECHCONTROLLER_H
#define EPISTEME_SPEECHCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

class AudioRecorder;
class InferenceService;

class SpeechController : public QObject {
  Q_OBJECT

public:
  // Where a capture was requested from. The composer in the assistant
  // shell and the editor in Normal mode are the only two destinations.
  // A capture is tagged when it begins and the tag travels with the
  // transcribed text so that only the originator acts on it.
  enum class Origin {
    Composer,
    Editor,
  };
  Q_ENUM(Origin)

  explicit SpeechController(InferenceService *inference,
                            QObject *parent = nullptr);
  ~SpeechController() override;

  bool beginCapture(Origin origin = Origin::Editor);
  void endCapture();
  void cancelCapture();
  bool isCapturing() const;
  bool isTranscribing() const;

  void startLiveCapture(Origin origin = Origin::Editor);
  void stopLiveCapture();
  bool isLiveCapturing() const;
  bool isLiveStarting() const;

  void speakText(const QString &text);
  void stopSpeaking();
  bool isSpeaking() const;

signals:
  void transcribed(const QString &text, SpeechController::Origin origin);
  void transcriptionFailed(const QString &error);

  // isFinal is true when the utterance is complete and the text will
  // not change again. false while the runtime may still refine it.
  void liveTranscribed(const QString &text, bool isFinal,
                       SpeechController::Origin origin);

  void speakingStarted();
  void speakingFinished();

  void stateChanged();

private slots:
  void onAudioChunkReady(const std::vector<float> &chunk);
  void onTranscriptionReady(const QString &text, const QString &error);
  void onLiveSegment(const QString &text, bool isFinal);
  void onStreamOpened();
  void onStreamClosed();
  void speakNextSentence();

private:
  InferenceService *m_inference = nullptr;
  std::unique_ptr<AudioRecorder> m_recorder;

  bool m_capturing = false;
  bool m_transcribing = false;
  bool m_liveCapturing = false;
  bool m_liveStarting = false;

  Origin m_captureOrigin = Origin::Editor;

  QStringList m_sentenceQueue;
  QString m_currentSentence;
  bool m_speaking = false;

  quint64 m_captureToken = 0;
};

#endif // EPISTEME_SPEECHCONTROLLER_H