#include "EditApplier.h"

#include <QTextCursor>

#include <algorithm>

EditApplier::EditApplier(QObject *parent) : QObject(parent) {}

bool EditApplier::apply(QTextDocument &document, const EditCommand &command,
                        const EditMatch &match) {
  if (!command.isValid()) {
    const QString reason = QStringLiteral("Invalid edit command.");

    emit failed(reason);

    return false;
  }

  if (!match.isValid()) {
    const QString reason = QStringLiteral("Invalid edit match.");

    emit failed(reason);

    return false;
  }

  QString reason;

  if (!applyOne(document, command, match, reason)) {
    emit failed(reason);

    return false;
  }

  emit applied(match.editDistance > 0, match.editDistance);

  return true;
}

bool EditApplier::applyBatch(QTextDocument &document,
                             const QVector<EditCommand> &commands,
                             const QVector<EditMatch> &matches) {
  if (commands.isEmpty()) {
    const QString reason = QStringLiteral("Edit batch is empty.");

    emit failed(reason);

    return false;
  }

  if (commands.size() != matches.size()) {
    const QString reason =
        QStringLiteral("Edit batch command/match count mismatch.");

    emit failed(reason);

    return false;
  }

  QVector<BatchEdit> edits;

  edits.reserve(commands.size());

  for (int i = 0; i < commands.size(); ++i) {
    const EditCommand &command = commands.at(i);

    const EditMatch &match = matches.at(i);

    if (!command.isValid()) {
      const QString reason =
          QStringLiteral("Edit %1 has an invalid command.").arg(i + 1);

      emit failed(reason);

      return false;
    }

    if (!match.isValid()) {
      const QString reason =
          QStringLiteral("Edit %1 has an invalid match.").arg(i + 1);

      emit failed(reason);

      return false;
    }

    edits.append(BatchEdit{i, command, match});
  }

  for (const BatchEdit &edit : edits) {
    const int start = edit.match.start;

    const int end = edit.match.end;

    if (start < 0 || end < start || end > document.toPlainText().size()) {
      const QString reason =
          QStringLiteral("Edit %1 has an out-of-range match.")
              .arg(edit.commandIndex + 1);

      emit failed(reason);

      return false;
    }

    if (edit.command.operation == EditCommand::Operation::Insert) {
      continue;
    }

    const QString currentText = document.toPlainText().mid(start, end - start);

    if (currentText != edit.match.matchedText) {
      const QString reason =
          QStringLiteral("Edit %1 became invalid before application.")
              .arg(edit.commandIndex + 1);

      emit failed(reason);

      return false;
    }
  }

  sortBatch(edits);

  int maximumEditDistance = 0;

  bool usedFuzzy = false;

  for (const BatchEdit &edit : edits) {
    QString reason;

    if (!applyOne(document, edit.command, edit.match, reason)) {
      const QString fullReason = QStringLiteral("Edit %1 failed: %2")
                                     .arg(edit.commandIndex + 1)
                                     .arg(reason);

      emit failed(fullReason);

      return false;
    }

    maximumEditDistance = qMax(maximumEditDistance, edit.match.editDistance);

    if (edit.match.editDistance > 0) {
      usedFuzzy = true;
    }
  }

  emit applied(usedFuzzy, maximumEditDistance);

  return true;
}

bool EditApplier::beginStreaming(QTextDocument &document,
                                 const EditCommand &command,
                                 const EditMatch &match) {
  if (m_streaming) {
    const QString reason =
        QStringLiteral("Another streaming edit is already active.");

    emit failed(reason);

    return false;
  }

  if (!command.isTargetValid()) {
    const QString reason = QStringLiteral("Invalid streaming edit target.");

    emit failed(reason);

    return false;
  }

  if (command.operation == EditCommand::Operation::Delete) {
    const QString reason =
        QStringLiteral("Delete operations cannot start content streaming.");

    emit failed(reason);

    return false;
  }

  if (!match.isValid()) {
    const QString reason = QStringLiteral("Invalid edit match.");

    emit failed(reason);

    return false;
  }

  const QString documentText = document.toPlainText();

  if (match.start < 0 || match.end < match.start ||
      match.end > documentText.size()) {
    const QString reason =
        QStringLiteral("Edit match is outside the document.");

    emit failed(reason);

    return false;
  }

  QString originalText;

  if (command.operation == EditCommand::Operation::Replace) {
    originalText = documentText.mid(match.start, match.end - match.start);

    if (originalText != match.matchedText) {
      const QString reason = QStringLiteral("Matched document text changed.");

      emit failed(reason);

      return false;
    }
  }

  m_streamingDocument = &document;

  m_streamingCommand = command;

  m_streamingMatch = match;

  m_streamingOriginalText = originalText;

  m_streamingStart = match.start;

  m_streamingLength = 0;

  m_streamingCursor = QTextCursor(&document);

  m_streamingCursor.setPosition(match.start);

  if (command.operation == EditCommand::Operation::Replace) {
    m_streamingCursor.setPosition(match.end, QTextCursor::KeepAnchor);

    m_streamingCursor.removeSelectedText();

    m_streamingCursor.setPosition(match.start);
  }

  m_streaming = true;

  return true;
}

bool EditApplier::appendStreaming(const QString &text) {
  if (!m_streaming || !m_streamingDocument) {
    const QString reason =
        QStringLiteral("EditApplier 1: No streaming edit is active.");

    emit failed(reason);

    return false;
  }

  if (text.isEmpty()) {
    return true;
  }

  m_streamingCursor.insertText(text);

  m_streamingLength += text.size();

  return true;
}

bool EditApplier::finishStreaming() {
  if (!m_streaming || !m_streamingDocument) {
    const QString reason =
        QStringLiteral("Edit Applier 2:No streaming edit is active.");

    emit failed(reason);

    return false;
  }

  if (m_streamingCommand.operation == EditCommand::Operation::Replace ||
      m_streamingCommand.operation == EditCommand::Operation::Insert) {
    if (m_streamingLength == 0) {
      const QString reason =
          QStringLiteral("Streaming edit produced no content.");

      cancelStreaming();

      emit failed(reason);

      return false;
    }
  }

  const bool fuzzy = m_streamingMatch.editDistance > 0;

  const int editDistance = m_streamingMatch.editDistance;

  m_streamingDocument = nullptr;

  m_streamingCursor = QTextCursor();

  m_streamingCommand = EditCommand();

  m_streamingMatch = EditMatch();

  m_streamingOriginalText.clear();

  m_streamingStart = -1;

  m_streamingLength = 0;

  m_streaming = false;

  emit applied(fuzzy, editDistance);

  return true;
}

void EditApplier::cancelStreaming() {
  if (!m_streaming || !m_streamingDocument) {
    return;
  }

  /*
   * The generated content occupies the range that starts
   * at m_streamingStart and currently has m_streamingLength
   * characters.
   */
  QTextCursor rollbackCursor = QTextCursor(m_streamingDocument);

  rollbackCursor.setPosition(m_streamingStart);

  rollbackCursor.setPosition(m_streamingStart + m_streamingLength,
                             QTextCursor::KeepAnchor);

  rollbackCursor.removeSelectedText();

  if (m_streamingCommand.operation == EditCommand::Operation::Replace &&
      !m_streamingOriginalText.isEmpty()) {
    rollbackCursor.insertText(m_streamingOriginalText);
  }

  m_streamingDocument = nullptr;

  m_streamingCursor = QTextCursor();

  m_streamingCommand = EditCommand();

  m_streamingMatch = EditMatch();

  m_streamingOriginalText.clear();

  m_streamingStart = -1;

  m_streamingLength = 0;

  m_streaming = false;
}

bool EditApplier::applyOne(QTextDocument &document, const EditCommand &command,
                           const EditMatch &match, QString &reason) {
  const QString documentText = document.toPlainText();

  if (match.start < 0 || match.end < match.start ||
      match.end > documentText.size()) {
    reason = QStringLiteral("Match range is outside the document.");

    return false;
  }

  QTextCursor cursor(&document);

  cursor.setPosition(match.start);

  switch (command.operation) {
  case EditCommand::Operation::Insert: {
    if (match.start != match.end) {
      reason = QStringLiteral("Insert match must have zero length.");

      return false;
    }

    cursor.insertText(command.newString);

    return true;
  }

  case EditCommand::Operation::Delete:
  case EditCommand::Operation::Replace:
    break;
  }

  const QString selectedText =
      documentText.mid(match.start, match.end - match.start);

  if (selectedText != match.matchedText) {
    reason = QStringLiteral("Matched document text changed.");

    return false;
  }

  cursor.setPosition(match.start);

  cursor.setPosition(match.end, QTextCursor::KeepAnchor);

  cursor.removeSelectedText();

  if (command.operation == EditCommand::Operation::Replace) {
    cursor.insertText(command.newString);
  }

  return true;
}

void EditApplier::sortBatch(QVector<BatchEdit> &edits) {
  std::stable_sort(edits.begin(), edits.end(),
                   [](const BatchEdit &a, const BatchEdit &b) {
                     if (a.match.start != b.match.start) {
                       return a.match.start > b.match.start;
                     }

                     if (a.match.end != b.match.end) {
                       return a.match.end > b.match.end;
                     }

                     return a.commandIndex > b.commandIndex;
                   });
}