#include "EditApplier.h"

#include <QTextCursor>
#include <QTextDocument>

EditApplier::EditApplier(QObject *parent) : QObject(parent) {}

bool EditApplier::apply(QTextDocument &document, const EditCommand &command,
                        const EditMatch &match) {
  if (!command.isValid()) {
    emit failed(QStringLiteral("Invalid edit command"));
    return false;
  }

  if (!match.isValid()) {
    emit failed(QStringLiteral("Invalid edit match"));
    return false;
  }

  if (match.start < 0 || match.end > document.characterCount()) {
    emit failed(QStringLiteral("Edit match is outside the document"));
    return false;
  }

  QTextCursor cursor(&document);

  cursor.beginEditBlock();

  cursor.setPosition(match.start);

  cursor.setPosition(match.end, QTextCursor::KeepAnchor);

  /*
   * Do not trust the cached candidate blindly.
   *
   * Verify that the selected range still contains the text that was
   * resolved against the document.
   *
   * For fuzzy matches this is the actual matched text, not necessarily
   * command.oldString.
   */
  const QString currentText = cursor.selectedText();

  if (match.editDistance == 0) {
    if (currentText != command.oldString) {
      cursor.endEditBlock();

      emit failed(
          QStringLiteral("Document changed before the exact edit was applied"));

      return false;
    }
  }

  cursor.insertText(command.newString);

  cursor.endEditBlock();

  emit applied(match.editDistance > 0, match.editDistance);

  return true;
}