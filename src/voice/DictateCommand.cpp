#include "../../include/voice/DictateCommand.h"

#include "../../include/text/TextEdit.h"
#include "../../include/voice/SpeechController.h"

#include <QDebug>
#include <QTextCursor>

DictateCommand::DictateCommand(SpeechController *controller, QObject *parent)
    : QObject(parent), m_controller(controller) {
  if (!m_controller) {
    return;
  }

  connect(m_controller, &SpeechController::transcribed, this,
          [this](const QString &text) {
            if (!m_target || text.isEmpty()) {
              m_target = nullptr;
              return;
            }

            QTextCursor cursor = m_target->textCursor();
            if (!cursor.isNull() && !text.isEmpty()) {
              // Ensure a space before the inserted text if the cursor is
              // not at the start of a block.
              const QString prefix =
                  cursor.atBlockStart() ? QString() : QStringLiteral(" ");
              cursor.insertText(prefix + text);
            } else {
              m_target->insertPlainText(text);
            }

            m_target = nullptr;
          });

  connect(m_controller, &SpeechController::transcriptionFailed, this,
          [this](const QString &error) {
            qWarning() << "[DictateCommand] Transcription failed:" << error;
            m_target = nullptr;
          });
}

QString DictateCommand::id() const { return QStringLiteral("dictate"); }

QString DictateCommand::title() const { return tr("Dictate"); }

QString DictateCommand::description() const {
  return tr("Speak into the current document. Release to insert.");
}

bool DictateCommand::canRun(const VoiceContext &context) const {
  return context.editor != nullptr && m_controller != nullptr;
}

bool DictateCommand::isRunning() const {
  return m_controller && (m_controller->isCapturing() ||
                          m_controller->isTranscribing());
}

void DictateCommand::start(const VoiceContext &context) {
  if (!m_controller || !context.editor) {
    return;
  }

  m_target = context.editor;
  m_controller->beginCapture();
}

void DictateCommand::stop() {
  if (m_controller) {
    m_controller->endCapture();
  }
}