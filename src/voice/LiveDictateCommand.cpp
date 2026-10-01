#include "../../include/voice/LiveDictateCommand.h"

#include "../../include/text/TextEdit.h"
#include "../../include/voice/SpeechController.h"

#include <QDebug>
#include <QTextCursor>

LiveDictateCommand::LiveDictateCommand(SpeechController *controller,
                                       QObject *parent)
    : QObject(parent), m_controller(controller) {
  if (!m_controller) return;

  connect(m_controller, &SpeechController::liveTranscribed, this,
          [this](const QString &text, bool isFinal,
                 SpeechController::Origin origin) {
            if (origin != SpeechController::Origin::Editor) {
              return;
            }

            onLiveTranscribed(text, isFinal);
          });

  connect(m_controller, &SpeechController::transcriptionFailed, this,
          [this](const QString &error) {
            qWarning() << "[LiveDictateCommand] Failed:" << error;
          });
}

QString LiveDictateCommand::id() const {
  return QStringLiteral("live_dictate");
}

QString LiveDictateCommand::title() const { return tr("Live Dictate"); }

QString LiveDictateCommand::description() const {
  return tr("Speak continuously. Text appears as the model refines.");
}

bool LiveDictateCommand::canRun(const VoiceContext &context) const {
  return context.editor != nullptr && m_controller != nullptr &&
         !m_controller->isCapturing() && !m_controller->isTranscribing() &&
         !m_controller->isLiveStarting() && !m_controller->isLiveCapturing();
}

bool LiveDictateCommand::isRunning() const {
  return m_controller &&
         (m_controller->isLiveStarting() || m_controller->isLiveCapturing());
}

void LiveDictateCommand::start(const VoiceContext &context) {
  if (!m_controller || !context.editor) return;
  m_target = context.editor;
  clearAnchor();
  m_controller->startLiveCapture(SpeechController::Origin::Editor);
}

void LiveDictateCommand::stop() {
  if (m_controller) m_controller->stopLiveCapture();
  m_target = nullptr;
  clearAnchor();
}

void LiveDictateCommand::clearAnchor() {
  m_anchorStart = -1;
  m_anchorLength = 0;
}

void LiveDictateCommand::onLiveTranscribed(const QString &text, bool isFinal) {
  if (!m_target || text.isEmpty()) {
    return;
  }

  QTextDocument *document = m_target->document();
  if (!document) {
    return;
  }

  const int docLength = document->characterCount() - 1;

  if (m_anchorStart < 0 || m_anchorStart > docLength) {
    clearAnchor();
  }

  QTextCursor cursor(document);

  if (m_anchorStart < 0) {
    QTextCursor editorCursor = m_target->textCursor();
    const bool atBlockStart = editorCursor.atBlockStart();
    const int position = editorCursor.position();

    cursor.setPosition(position);

    const QString payload = atBlockStart ? text : QStringLiteral(" ") + text;
    cursor.insertText(payload);

    m_anchorStart = position + (atBlockStart ? 0 : 1);
    m_anchorLength = text.length();
  } else {
    const int replaceEnd =
        qMin(m_anchorStart + m_anchorLength, docLength);

    cursor.setPosition(m_anchorStart);
    cursor.setPosition(replaceEnd, QTextCursor::KeepAnchor);
    cursor.insertText(text);

    m_anchorLength = text.length();
  }

  const int endPosition = m_anchorStart + m_anchorLength;

  QTextCursor visible(document);
  visible.setPosition(endPosition);
  m_target->setTextCursor(visible);

  if (isFinal) {
    clearAnchor();

    QTextCursor park(document);
    park.setPosition(endPosition);
    m_target->setTextCursor(park);
  }
}