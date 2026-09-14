#include "EditApplier.h"

#include "TextDocument.h"

#include <QTextCursor>

#include <algorithm>

namespace {

QString invalidCommandError() {
  return QStringLiteral("Invalid edit command.");
}

QString invalidMatchError() { return QStringLiteral("Invalid edit match."); }

bool isValidRange(const EditMatch &match, int documentLength) {
  return match.start >= 0 && match.end >= match.start &&
         match.end <= documentLength;
}

bool matchesDocumentText(const QString &documentText, const EditMatch &match) {
  if (!isValidRange(match, documentText.size())) {
    return false;
  }

  return documentText.mid(match.start, match.end - match.start) ==
         match.matchedText;
}

} // namespace

EditApplier::EditApplier(QObject *parent) : QObject(parent) {}

bool EditApplier::apply(QTextDocument &document, const EditCommand &command,
                        const EditMatch &match) {
  if (!command.isValid()) {
    emit failed(invalidCommandError());
    return false;
  }

  if (!match.isValid()) {
    emit failed(invalidMatchError());
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
    emit failed(QStringLiteral("Edit batch is empty."));
    return false;
  }

  if (commands.size() != matches.size()) {
    emit failed(QStringLiteral("Edit batch command/match count mismatch."));
    return false;
  }

  QVector<BatchEdit> edits;
  edits.reserve(commands.size());

  for (int index = 0; index < commands.size(); ++index) {
    const EditCommand &command = commands.at(index);
    const EditMatch &match = matches.at(index);

    if (!command.isValid()) {
      emit failed(
          QStringLiteral("Edit %1 has an invalid command.").arg(index + 1));

      return false;
    }

    if (!match.isValid()) {
      emit failed(
          QStringLiteral("Edit %1 has an invalid match.").arg(index + 1));

      return false;
    }

    edits.append(BatchEdit{index, command, match});
  }

  const QString documentText = document.toPlainText();

  for (const BatchEdit &edit : edits) {
    if (!isValidRange(edit.match, documentText.size())) {
      emit failed(QStringLiteral("Edit %1 has an out-of-range match.")
                      .arg(edit.commandIndex + 1));

      return false;
    }

    if (edit.command.operation == EditCommand::Operation::Insert) {
      if (edit.match.start != edit.match.end) {
        emit failed(QStringLiteral("Edit %1 has an invalid insertion range.")
                        .arg(edit.commandIndex + 1));

        return false;
      }

      continue;
    }

    if (!matchesDocumentText(documentText, edit.match)) {
      emit failed(QStringLiteral("Edit %1 became invalid before application.")
                      .arg(edit.commandIndex + 1));

      return false;
    }
  }

  sortBatch(edits);

  int maximumEditDistance = 0;
  bool usedFuzzyMatch = false;

  for (const BatchEdit &edit : edits) {
    QString reason;

    if (!applyOne(document, edit.command, edit.match, reason)) {
      emit failed(QStringLiteral("Edit %1 failed: %2")
                      .arg(edit.commandIndex + 1)
                      .arg(reason));

      return false;
    }

    maximumEditDistance = qMax(maximumEditDistance, edit.match.editDistance);

    usedFuzzyMatch = usedFuzzyMatch || edit.match.editDistance > 0;
  }

  emit applied(usedFuzzyMatch, maximumEditDistance);
  return true;
}

bool EditApplier::beginStreaming(QTextDocument &document,
                                 const EditCommand &command,
                                 const EditMatch &match) {
  if (m_streaming) {
    emit failed(QStringLiteral("Another streaming edit is already active."));
    return false;
  }

  if (!command.isTargetValid()) {
    emit failed(QStringLiteral("Invalid streaming edit target."));
    return false;
  }

  if (command.operation == EditCommand::Operation::Delete) {
    emit failed(
        QStringLiteral("Delete operations cannot start content streaming."));
    return false;
  }

  if (!match.isValid()) {
    emit failed(invalidMatchError());
    return false;
  }

  const QString documentText = document.toPlainText();

  if (!isValidRange(match, documentText.size())) {
    emit failed(QStringLiteral("Edit match is outside the document."));
    return false;
  }

  QString originalText;

  if (command.operation == EditCommand::Operation::Replace ||
      command.operation == EditCommand::Operation::ReplaceScope) {
    originalText = documentText.mid(match.start, match.end - match.start);

    if (originalText != match.matchedText) {
      emit failed(QStringLiteral("Matched document text changed."));
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

  if (command.operation == EditCommand::Operation::Replace ||
      command.operation == EditCommand::Operation::ReplaceScope) {
    m_streamingCursor.setPosition(match.end, QTextCursor::KeepAnchor);

    m_streamingCursor.removeSelectedText();
    m_streamingCursor.setPosition(match.start);
  }

  m_streaming = true;
  return true;
}

bool EditApplier::appendStreaming(const QString &text) {
  if (!m_streaming || !m_streamingDocument) {
    emit failed(QStringLiteral("No streaming edit is active."));
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
    emit failed(QStringLiteral("No streaming edit is active."));
    return false;
  }

  const bool requiresContent =
      m_streamingCommand.operation == EditCommand::Operation::Insert ||
      m_streamingCommand.operation == EditCommand::Operation::Replace ||
      m_streamingCommand.operation == EditCommand::Operation::ReplaceScope;

  if (requiresContent && m_streamingLength == 0) {
    const QString reason =
        QStringLiteral("Streaming edit produced no content.");

    cancelStreaming();
    emit failed(reason);

    return false;
  }

  const bool usedFuzzyMatch = m_streamingMatch.editDistance > 0;

  const int editDistance = m_streamingMatch.editDistance;

  resetStreamingState();

  emit applied(usedFuzzyMatch, editDistance);
  return true;
}

void EditApplier::cancelStreaming() {
  if (!m_streaming || !m_streamingDocument) {
    return;
  }

  QTextCursor cursor(m_streamingDocument);

  cursor.setPosition(m_streamingStart);
  cursor.setPosition(m_streamingStart + m_streamingLength,
                     QTextCursor::KeepAnchor);

  cursor.removeSelectedText();

  const bool restoreOriginal =
      (m_streamingCommand.operation == EditCommand::Operation::Replace ||
       m_streamingCommand.operation == EditCommand::Operation::ReplaceScope) &&
      !m_streamingOriginalText.isEmpty();

  if (restoreOriginal) {
    cursor.insertText(m_streamingOriginalText);
  }

  resetStreamingState();
}

bool EditApplier::applyOne(QTextDocument &document, const EditCommand &command,
                           const EditMatch &match, QString &reason) {
  const QString documentText = document.toPlainText();

  if (!isValidRange(match, documentText.size())) {
    reason = QStringLiteral("Match range is outside the document.");

    return false;
  }

  if (command.operation == EditCommand::Operation::Insert) {
    if (match.start != match.end) {
      reason = QStringLiteral("Insert match must have zero length.");

      return false;
    }

    QTextCursor cursor(&document);
    cursor.setPosition(match.start);
    cursor.insertText(command.newString);

    return true;
  }

  if (!matchesDocumentText(documentText, match)) {
    reason = QStringLiteral("Matched document text changed.");

    return false;
  }

  QTextCursor cursor(&document);
  cursor.setPosition(match.start);
  cursor.setPosition(match.end, QTextCursor::KeepAnchor);

  cursor.removeSelectedText();

  if (command.operation == EditCommand::Operation::Replace ||
      command.operation == EditCommand::Operation::ReplaceScope) {
    cursor.insertText(command.newString);
  }

  return true;
}

void EditApplier::resetStreamingState() {
  m_streamingDocument = nullptr;
  m_streamingCursor = QTextCursor();
  m_streamingCommand = EditCommand();
  m_streamingMatch = EditMatch();
  m_streamingOriginalText.clear();
  m_streamingStart = -1;
  m_streamingLength = 0;
  m_streaming = false;
}

void EditApplier::sortBatch(QVector<BatchEdit> &edits) {
  std::stable_sort(edits.begin(), edits.end(),
                   [](const BatchEdit &left, const BatchEdit &right) {
                     if (left.match.start != right.match.start) {
                       return left.match.start > right.match.start;
                     }

                     if (left.match.end != right.match.end) {
                       return left.match.end > right.match.end;
                     }

                     return left.commandIndex > right.commandIndex;
                   });
}