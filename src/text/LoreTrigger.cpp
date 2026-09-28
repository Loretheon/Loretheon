#include "../../include/text/LoreTrigger.h"

#include "../../include/text/TextEdit.h"

#include <QDebug>
#include <QTextCursor>

namespace {

const QString kTrigger = QStringLiteral("Search");

} // namespace

LoreTrigger::LoreTrigger(TextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(editor) {
  if (!m_editor) {
    return;
  }

  connect(m_editor, &QPlainTextEdit::textChanged, this,
          &LoreTrigger::onContentsChanged);
}

LoreTrigger::~LoreTrigger() = default;

void LoreTrigger::onContentsChanged() {
  if (m_processing || !m_editor) {
    return;
  }

  QTextCursor cursor = m_editor->textCursor();

  // Only fire if the caret is at the end of a partial "@Lore" that is
  // not already followed by a space. If it is "@Lore " with the space,
  // that is the moment to fire and remove the whole trigger.
  const QString documentText = m_editor->toPlainText();
  const int position = cursor.position();

  if (position < kTrigger.length()) {
    return;
  }

  const int triggerStart = position - kTrigger.length();

  if (documentText.mid(triggerStart, kTrigger.length()) != kTrigger) {
    return;
  }

  // Require a word boundary before the trigger so we do not fire inside
  // "@LoremIpsum".
  if (triggerStart > 0) {
    const QChar before = documentText.at(triggerStart - 1);
    if (before.isLetterOrNumber()) {
      return;
    }
  }

  // Require that the character after the caret, if any, is whitespace
  // or end-of-document. That means the user has finished typing the
  // trigger word and either hit space or is at the end of the line.
  const bool atEnd = position >= documentText.size();
  const bool followedBySpace =
      !atEnd && documentText.at(position).isSpace();

  if (!atEnd && !followedBySpace) {
    return;
  }

  // Remove the trigger text plus any single space that follows it.
  m_processing = true;

  int removeEnd = position;
  if (followedBySpace) {
    ++removeEnd;
  }

  QTextCursor editor = m_editor->textCursor();
  editor.setPosition(triggerStart);
  editor.setPosition(removeEnd, QTextCursor::KeepAnchor);
  editor.removeSelectedText();

  m_editor->setTextCursor(editor);

  m_processing = false;

  emit triggerDetected(triggerStart);
}