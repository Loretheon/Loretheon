#include "../../include/voice/ReadAloudCommand.h"

#include "../../include/text/TextEdit.h"
#include "../../include/voice/SpeechController.h"

#include <QPlainTextEdit>

ReadAloudCommand::ReadAloudCommand(SpeechController *controller,
                                   QObject *parent)
    : QObject(parent), m_controller(controller) {}

QString ReadAloudCommand::id() const { return QStringLiteral("read_aloud"); }

QString ReadAloudCommand::title() const { return tr("Read Aloud"); }

QString ReadAloudCommand::description() const {
  return tr("Read the current document aloud.");
}

bool ReadAloudCommand::canRun(const VoiceContext &context) const {
  return context.editor != nullptr && m_controller != nullptr &&
         !context.editor->toPlainText().trimmed().isEmpty();
}

bool ReadAloudCommand::isRunning() const {
  return m_controller && m_controller->isSpeaking();
}

void ReadAloudCommand::start(const VoiceContext &context) {
  if (!m_controller || !context.editor) {
    return;
  }

  const QString text = context.editor->toPlainText().trimmed();
  if (text.isEmpty()) {
    return;
  }

  m_controller->speakText(text);
}

void ReadAloudCommand::stop() {
  if (m_controller) {
    m_controller->stopSpeaking();
  }
}