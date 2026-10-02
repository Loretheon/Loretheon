#include "../../include/assistant/ConversationMode.h"

#include "../../include/assistant/LoreAssistant.h"
#include "../../include/voice/SpeechController.h"

#include <QDebug>

ConversationMode::ConversationMode(LoreAssistant *assistant,
                                   SpeechController *speech,
                                   QObject *parent)
    : QObject(parent), m_assistant(assistant), m_speech(speech) {
  m_silenceTimer = new QTimer(this);
  m_silenceTimer->setSingleShot(true);
  m_silenceTimer->setInterval(m_silenceMs);

  connect(m_silenceTimer, &QTimer::timeout, this,
          &ConversationMode::onSilenceElapsed);

  if (m_speech) {
    connect(m_speech, &SpeechController::liveTranscribed, this,
            &ConversationMode::onLiveTranscribed);

    connect(m_speech, &SpeechController::speakingFinished, this,
            &ConversationMode::onSpeakingFinished);

    connect(m_speech, &SpeechController::sttStreamClosed, this,
            &ConversationMode::onStreamClosed);
  }

  if (m_assistant) {
    connect(m_assistant, &LoreAssistant::assistantTurnFinished, this,
            &ConversationMode::onAssistantTurnFinished);
  }
}

ConversationMode::~ConversationMode() { stop(); }

void ConversationMode::setSilenceMs(int ms) {
  m_silenceMs = qBound(500, ms, 5000);
  m_silenceTimer->setInterval(m_silenceMs);
}

void ConversationMode::start() {
  if (m_state != State::Off) {
    return;
  }

  beginListening();
}

void ConversationMode::stop() {
  if (m_state == State::Off) {
    return;
  }

  m_silenceTimer->stop();

  if (m_speech && m_speech->isLiveCapturing()) {
    m_speech->stopLiveCapture();
  }

  if (m_speech && m_speech->isSpeaking()) {
    m_speech->stopSpeaking();
  }

  m_buffer.clear();
  m_lastSegmentText.clear();
  emit transcriptChanged(QString());

  setState(State::Off);
}

void ConversationMode::beginListening() {
  if (!m_speech) {
    setState(State::Off);
    return;
  }

  m_buffer.clear();
  m_lastSegmentText.clear();
  emit transcriptChanged(QString());

  if (m_speech->isLiveCapturing() || m_speech->isLiveStarting()) {
    m_speech->stopLiveCapture();
  }

  if (m_speech->isCapturing() || m_speech->isTranscribing()) {
    m_speech->cancelCapture();
  }

  m_speech->startLiveCapture(SpeechController::Origin::Conversation);

  m_silenceTimer->stop();

  setState(State::Listening);
}

void ConversationMode::commitTurn() {
  const QString text = m_buffer.trimmed();

  m_buffer.clear();
  m_lastSegmentText.clear();
  emit transcriptChanged(QString());

  if (text.isEmpty()) {
    beginListening();
    return;
  }

  if (m_speech && m_speech->isLiveCapturing()) {
    m_speech->stopLiveCapture();
  }

  setState(State::Thinking);

  emit messageCommitted(text);

  if (m_assistant) {
    m_assistant->handleUserMessage(text);
  }
}

void ConversationMode::onLiveTranscribed(
    const QString &text, bool isFinal,
    SpeechController::Origin origin) {
  if (m_state != State::Listening) {
    return;
  }

  if (origin == SpeechController::Origin::Composer) {
    return;
  }

  if (text.isEmpty()) {
    return;
  }

  m_buffer = text;
  emit transcriptChanged(m_buffer);

  if (text != m_lastSegmentText) {
    qDebug() << "[ConversationMode::onLiveTranscribed] New segment:" << text
             << "restarting timer";
    m_lastSegmentText = text;
    m_silenceTimer->start();
  }
}

void ConversationMode::onSilenceElapsed() {
  qDebug() << "[ConversationMode::onSilenceElapsed] FIRED";

  if (m_state != State::Listening) {
    return;
  }

  commitTurn();
}

void ConversationMode::onAssistantTurnFinished(const QString &nodeId) {
  Q_UNUSED(nodeId);

  if (m_state != State::Thinking) {
    return;
  }

  if (!m_assistant || !m_speech) {
    beginListening();
    return;
  }

  const QString reply = m_assistant->lastReply();

  if (reply.trimmed().isEmpty()) {
    beginListening();
    return;
  }

  setState(State::Speaking);

  m_speech->speakText(reply);

  if (!m_speech->isSpeaking()) {
    beginListening();
  }
}

void ConversationMode::onSpeakingFinished() {
  if (m_state != State::Speaking) {
    return;
  }

  beginListening();
}

void ConversationMode::onStreamClosed() {
  m_silenceTimer->stop();
}

void ConversationMode::setState(State state) {
  if (m_state == state) {
    return;
  }

  qDebug() << "[Conversation] setState" << static_cast<int>(m_state)
           << "->" << static_cast<int>(state);

  m_state = state;
  emit stateChanged(m_state);
}