#pragma once

#include "SpeechController.h"

#include <QObject>
#include <QTimer>

class LoreAssistant;
class SpeechController;

class ConversationMode : public QObject {
  Q_OBJECT

public:
  enum class State {
    Off = 0,
    Listening = 1,
    Thinking = 2,
    Speaking = 3,
  };

  explicit ConversationMode(LoreAssistant *assistant,
                            SpeechController *speech,
                            QObject *parent = nullptr);
  ~ConversationMode() override;

  void setSilenceMs(int ms);

  void start();
  void stop();

  State state() const { return m_state; }
  bool isActive() const { return m_state != State::Off; }

  signals:
    void stateChanged(State state);
  void transcriptChanged(const QString &text);
  void messageCommitted(const QString &text);

private slots:
  void beginListening();
  void commitTurn();
  void onLiveTranscribed(const QString &text, bool isFinal,
                         SpeechController::Origin origin);
  void onSilenceElapsed();
  void onAssistantTurnFinished(const QString &nodeId);
  void onSpeakingFinished();
  void onStreamClosed();

private:
  void setState(State state);

  LoreAssistant *m_assistant = nullptr;
  SpeechController *m_speech = nullptr;

  State m_state = State::Off;

  QString m_buffer;
  QString m_lastSegmentText;
  QTimer *m_silenceTimer = nullptr;
  int m_silenceMs = 800;
};