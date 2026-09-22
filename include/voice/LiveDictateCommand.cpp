#include "../../include/voice/LiveDictateCommand.h"

#include "../../include/voice/SpeechController.h"
#include "../../include/text/TextEdit.h"

#include <QDebug>
#include <QTextCursor>

LiveDictateCommand::LiveDictateCommand(SpeechController *controller,
                                       QObject *parent)
    : QObject(parent), m_controller(controller) {
  if (!m_controller) return;

  connect(m_controller, &SpeechController::liveTranscribed, this,
          &LiveDictateCommand::onLiveTranscribed);

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
  m_controller->startLiveCapture();
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

  // Sanity: a stale anchor that reaches past the document means the
  // user edited underneath us. Abandon it and start fresh.
  if (m_anchorStart < 0 || m_anchorStart > docLength) {
    clearAnchor();
  }

  QTextCursor cursor(document);

  if (m_anchorStart < 0) {
    // First result of an utterance. Insert at the editor's cursor and
    // remember where the interim begins.
    QTextCursor editorCursor = m_target->textCursor();
    const bool atBlockStart = editorCursor.atBlockStart();
    const int position = editorCursor.position();

    cursor.setPosition(position);

    const QString payload = atBlockStart ? text : QStringLiteral(" ") + text;
    cursor.insertText(payload);

    // The anchor covers only the text, not the leading space.
    m_anchorStart = position + (atBlockStart ? 0 : 1);
    m_anchorLength = text.length();
  } else {
    // Subsequent result. Replace the previous interim text with the new
    // one. The new one is always longer or equal for a given utterance,
    // but we handle both cases.
    const int replaceEnd =
        qMin(m_anchorStart + m_anchorLength, docLength);

    cursor.setPosition(m_anchorStart);
    cursor.setPosition(replaceEnd, QTextCursor::KeepAnchor);
    cursor.insertText(text);

    m_anchorLength = text.length();
  }

  const int endPosition = m_anchorStart + m_anchorLength;

  // Always move the editor's visible cursor to the end of the interim
  // so that typing (if any) and the next insert land after it.
  QTextCursor visible(document);
  visible.setPosition(endPosition);
  m_target->setTextCursor(visible);

  if (isFinal) {
    // Utterance is complete. Clear the anchor so the next result starts
    // a fresh insert after this one.
    clearAnchor();

    // Park the cursor after the final text so the next utterance does
    // not stomp on it.
    QTextCursor park(document);
    park.setPosition(endPosition);
    m_target->setTextCursor(park);
  }
}