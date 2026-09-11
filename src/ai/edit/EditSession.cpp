#include "EditSession.h"

#include "EditApplier.h"
#include "EditCandidateView.h"

#include "../../text/TextEdit.h"
#include "../../text/model/TextDocument.h"

#include <algorithm>

namespace {

constexpr int InvalidRevision = -1;
constexpr int InvalidPendingEditId = 0;

QString noActiveEditorError() { return QStringLiteral("No active editor."); }

QString invalidDocumentError() {
  return QStringLiteral("Active editor does not use TextDocument.");
}

} // namespace

EditSession::EditSession(TextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(editor), m_applier(new EditApplier(this)),
      m_candidateView(new EditCandidateView()) {
  connect(m_candidateView, &EditCandidateView::candidateSelected, this,
          &EditSession::onCandidateSelected);

  connect(m_applier, &EditApplier::applied, this, &EditSession::applied);

  connect(m_applier, &EditApplier::failed, this, &EditSession::failed);
}

void EditSession::setEditor(TextEdit *editor) {
  if (m_editor == editor) {
    return;
  }

  abort();
  m_editor = editor;
}

bool EditSession::hasConflicts() const {
  return std::any_of(m_pendingEdits.cbegin(), m_pendingEdits.cend(),
                     [](const PendingEdit &edit) { return edit.hasConflict; });
}

void EditSession::detectConflicts() {
  for (PendingEdit &edit : m_pendingEdits) {
    edit.hasConflict = false;
  }

  for (int first = 0; first < m_pendingEdits.size(); ++first) {
    const EditMatch &firstMatch = m_pendingEdits.at(first).match;

    if (!firstMatch.isValid()) {
      continue;
    }

    for (int second = first + 1; second < m_pendingEdits.size(); ++second) {
      const EditMatch &secondMatch = m_pendingEdits.at(second).match;

      if (!secondMatch.isValid()) {
        continue;
      }

      const bool overlaps = firstMatch.start < secondMatch.end &&
                            firstMatch.end > secondMatch.start;

      if (!overlaps) {
        continue;
      }

      m_pendingEdits[first].hasConflict = true;
      m_pendingEdits[second].hasConflict = true;
    }
  }
}

bool EditSession::resolvePlan(const QJsonArray &planArray) {
  abort();

  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  document->rebuildStructure();
  m_documentRevision = document->revision();

  QVector<EditCommand> commands;
  commands.reserve(planArray.size());

  for (const QJsonValue &value : planArray) {
    if (!value.isObject()) {
      continue;
    }

    const EditCommand command = EditCommand::fromJson(value.toObject());

    if (command.isCommandValid()) {
      commands.append(command);
    }
  }

  if (commands.isEmpty()) {
    emit failed(QStringLiteral("Plan contains no valid edit commands."));
    return false;
  }

  m_pendingEdits.reserve(m_pendingEdits.size() + commands.size());

  for (int index = 0; index < commands.size(); ++index) {
    EditMatch match;

    if (!resolveCommand(commands.at(index), match)) {
      continue;
    }

    PendingEdit edit;
    edit.id = index + 1;
    edit.command = commands.at(index);
    edit.match = match;
    edit.accepted = true;
    edit.completed = false;

    m_pendingEdits.append(edit);
  }

  detectConflicts();

  if (hasConflicts()) {
    emit conflictsDetected();
  }

  emit planReady(commands);
  return true;
}

bool EditSession::propose(const EditCommand &command) {
  abort();

  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  if (!command.isCommandValid()) {
    emit failed(QStringLiteral("Invalid edit command."));
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  setState(State::Matching);

  document->rebuildStructure();
  m_documentRevision = document->revision();

  m_pendingCommand = command;
  m_pendingCandidates.clear();

  EditMatcher::Result result;

  if (command.operation == EditCommand::Operation::Insert) {
    EditMatch match;

    if (!createInsertionMatch(command, match)) {
      setState(State::Idle);
      return false;
    }

    result.candidates.append(match);
  } else {
    result = m_matcher.find(*document, command);
  }

  if (result.candidates.isEmpty()) {
    setState(State::Idle);

    emit failed(QStringLiteral("No matching text found in scope '%1'.")
                    .arg(command.scopeId));

    return false;
  }

  m_pendingCandidates = result.candidates;

  if (m_pendingCandidates.size() == 1) {
    applyCandidate(m_pendingCandidates.first());
    return true;
  }

  setState(State::AwaitingSelection);
  emit candidatesReady(m_pendingCandidates);
  return true;
}

bool EditSession::proposeMany(const QVector<EditCommand> &commands) {
  abort();

  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  if (commands.isEmpty()) {
    emit failed(QStringLiteral("Edit batch is empty."));
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  for (int index = 0; index < commands.size(); ++index) {
    if (commands.at(index).isValid()) {
      continue;
    }

    emit failed(QStringLiteral("Edit %1 is invalid.").arg(index + 1));
    return false;
  }

  setState(State::Matching);

  document->rebuildStructure();
  m_documentRevision = document->revision();

  QVector<EditMatch> matches;

  if (!resolveBatch(commands, matches)) {
    setState(State::Idle);
    return false;
  }

  setState(State::Applying);

  const bool applied = m_applier->applyBatch(*document, commands, matches);

  m_pendingCandidates.clear();
  setState(State::Idle);

  return applied;
}

bool EditSession::prepareStreaming(const EditCommand &command,
                                   int pendingEditId) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  if (!command.isCommandValid()) {
    emit failed(QStringLiteral("Invalid structural edit command."));
    return false;
  }

  if (m_applier && m_applier->isStreaming()) {
    m_applier->cancelStreaming();
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  document->rebuildStructure();
  m_documentRevision = document->revision();

  m_pendingCommand = command;
  m_pendingCandidates.clear();

  setState(State::Matching);

  EditMatch match;

  if (!resolveCommand(command, match)) {
    setState(State::Idle);
    return false;
  }

  if (document->revision() != m_documentRevision) {
    setState(State::Idle);
    emit failed(QStringLiteral("Document changed before streaming started."));
    return false;
  }

  m_pendingMatch = match;
  m_currentPendingEditId = pendingEditId;

  PendingEdit *pendingEdit = nullptr;

  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.id != pendingEditId) {
      continue;
    }

    pendingEdit = &edit;
    break;
  }

  if (!pendingEdit) {
    PendingEdit edit;
    edit.id = pendingEditId;
    edit.command = command;
    edit.match = match;
    edit.generatedText.clear();
    edit.accepted = true;
    edit.completed = false;

    m_pendingEdits.append(edit);
    pendingEdit = &m_pendingEdits.last();
  } else {
    pendingEdit->command = command;
    pendingEdit->match = match;
    pendingEdit->generatedText.clear();
    pendingEdit->accepted = true;
    pendingEdit->completed = false;
    pendingEdit->hasConflict = false;
  }

  emit pendingEditStarted(*pendingEdit);

  if (command.operation == EditCommand::Operation::Delete) {
    pendingEdit->completed = true;

    emit pendingEditFinished(*pendingEdit);

    m_pendingMatch = EditMatch();
    m_pendingCommand = EditCommand();
    m_currentPendingEditId = InvalidPendingEditId;

    setState(State::WaitingForReview);

    emit pendingEditsChanged();
    emit reviewReady();

    return true;
  }

  setState(State::Streaming);
  return true;
}

bool EditSession::appendStreaming(const QString &text) {
  if (m_state != State::Streaming) {
    emit failed(QStringLiteral("No streaming edit is active."));
    return false;
  }

  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.id != m_currentPendingEditId) {
      continue;
    }

    edit.generatedText += text;
    emit pendingEditUpdated(edit);
    return true;
  }

  emit failed(QStringLiteral("Pending edit was not found."));
  return false;
}

bool EditSession::finishStreaming() {
  if (m_state != State::Streaming) {
    emit failed(QStringLiteral("No streaming edit is active."));
    return false;
  }

  PendingEdit *pendingEdit = nullptr;

  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.id != m_currentPendingEditId) {
      continue;
    }

    pendingEdit = &edit;
    break;
  }

  if (!pendingEdit) {
    emit failed(QStringLiteral("Pending edit was not found."));
    return false;
  }

  if (pendingEdit->command.operation != EditCommand::Operation::Delete &&
      pendingEdit->generatedText.isEmpty()) {
    emit failed(QStringLiteral("Streaming edit produced no content."));
    return false;
  }

  pendingEdit->command.newString = pendingEdit->generatedText;
  pendingEdit->completed = true;

  emit pendingEditFinished(*pendingEdit);

  m_pendingMatch = EditMatch();
  m_pendingCommand = EditCommand();
  m_currentPendingEditId = InvalidPendingEditId;

  setState(State::WaitingForReview);
  emit reviewReady();

  return true;
}

void EditSession::abort() {
  if (m_applier && m_applier->isStreaming()) {
    m_applier->cancelStreaming();
  }

  const bool wasActive = m_state != State::Idle;

  m_pendingCandidates.clear();
  m_pendingEdits.clear();
  m_pendingMatch = EditMatch();
  m_pendingCommand = EditCommand();
  m_currentPendingEditId = InvalidPendingEditId;
  m_documentRevision = InvalidRevision;

  setState(State::Idle);

  if (wasActive) {
    emit aborted();
  }
}

void EditSession::setState(State state) {
  if (m_state == state) {
    return;
  }

  m_state = state;
  emit stateChanged(m_state);
}

void EditSession::applyCandidate(const EditMatch &match) {
  if (!m_editor) {
    setState(State::Idle);
    emit failed(noActiveEditorError());
    return;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    setState(State::Idle);
    emit failed(invalidDocumentError());
    return;
  }

  if (document->revision() != m_documentRevision) {
    m_pendingCandidates.clear();
    setState(State::Idle);

    emit failed(
        QStringLiteral("Document changed before the edit was applied."));

    return;
  }

  if (!match.isValid()) {
    setState(State::Idle);
    emit failed(QStringLiteral("Invalid edit match."));
    return;
  }

  setState(State::Applying);

  m_applier->apply(*document, m_pendingCommand, match);

  m_pendingCandidates.clear();
  setState(State::Idle);
}

bool EditSession::resolveCommand(const EditCommand &command, EditMatch &match) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  if (command.operation == EditCommand::Operation::Insert) {
    return createInsertionMatch(command, match);
  }

  const EditMatcher::Result result = m_matcher.find(*document, command);

  if (result.candidates.isEmpty()) {
    emit failed(QStringLiteral("No matching text found in scope '%1'.")
                    .arg(command.scopeId));

    return false;
  }

  if (result.candidates.size() > 1 && !command.replaceAll) {
    emit failed(QStringLiteral("Edit has %1 possible matches in scope '%2'.")
                    .arg(result.candidates.size())
                    .arg(command.scopeId));

    return false;
  }

  match = result.candidates.first();
  return true;
}

bool EditSession::resolveSingle(const EditCommand &command, EditMatch &match) {
  return resolveCommand(command, match);
}

bool EditSession::resolveBatch(const QVector<EditCommand> &commands,
                               QVector<EditMatch> &matches) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  matches.clear();
  matches.reserve(commands.size());

  for (int index = 0; index < commands.size(); ++index) {
    EditMatch match;

    if (resolveSingle(commands.at(index), match)) {
      matches.append(match);
      continue;
    }

    emit failed(
        QStringLiteral("Edit %1 could not be resolved.").arg(index + 1));

    return false;
  }

  if (document->revision() != m_documentRevision) {
    emit failed(QStringLiteral("Document changed while resolving edits."));
    return false;
  }

  return true;
}

bool EditSession::createInsertionMatch(const EditCommand &command,
                                       EditMatch &match) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  document->rebuildStructure();

  const QString documentText = document->toPlainText();
  const int documentLength = documentText.size();

  // An empty scope represents the synthetic document root.
  if (command.scopeId.isEmpty()) {
    if (!documentText.isEmpty()) {
      emit failed(
          QStringLiteral("Empty insertion scope is only valid for an empty "
                         "document."));

      return false;
    }

    if (!command.findString.isEmpty()) {
      emit failed(
          QStringLiteral("The empty document root cannot have an insertion "
                         "anchor."));

      return false;
    }

    match.start = 0;
    match.end = 0;
    match.highlightStart = 0;
    match.highlightEnd = documentLength;
    match.editDistance = 0;
    match.matchedText.clear();

    return true;
  }

  const DocumentStructure &structure = document->structure();
  const DocumentNode *scope = structure.find(command.scopeId);

  if (!scope) {
    emit failed(QStringLiteral("Insertion scope '%1' was not found.")
                    .arg(command.scopeId));

    return false;
  }

  const int scopeStart = scope->start;
  const int scopeEnd = scope->end;

  const bool validStart = scopeStart >= 0 && scopeStart <= documentLength;

  const bool validEnd = scopeEnd >= scopeStart && scopeEnd <= documentLength;

  if (!validStart || !validEnd) {
    emit failed(QStringLiteral("Insertion scope '%1' has an invalid range.")
                    .arg(command.scopeId));

    return false;
  }

  if (!command.findString.isEmpty()) {
    const QString scopeText =
        documentText.mid(scopeStart, scopeEnd - scopeStart);

    const int anchorOffset = scopeText.indexOf(command.findString);

    if (anchorOffset < 0) {
      emit failed(
          QStringLiteral("Insertion anchor was not found in scope '%1'.")
              .arg(command.scopeId));

      return false;
    }

    const int anchorStart = scopeStart + anchorOffset;
    const int anchorEnd = anchorStart + command.findString.size();

    const int insertionPosition =
        command.position == EditCommand::Position::Before ? anchorStart
                                                          : anchorEnd;

    match.start = insertionPosition;
    match.end = insertionPosition;
    match.highlightStart = scopeStart;
    match.highlightEnd = scopeEnd;
    match.editDistance = 0;
    match.matchedText = command.findString;

    return true;
  }

  const int insertionPosition =
      command.position == EditCommand::Position::After ? scopeEnd : scopeStart;

  match.start = insertionPosition;
  match.end = insertionPosition;
  match.highlightStart = scopeStart;
  match.highlightEnd = scopeEnd;
  match.editDistance = 0;
  match.matchedText.clear();

  return true;
}

bool EditSession::setPendingEditAccepted(int id, bool accepted) {
  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.id != id) {
      continue;
    }

    edit.accepted = accepted;

    emit pendingEditUpdated(edit);
    emit pendingEditsChanged();

    return true;
  }

  return false;
}

void EditSession::acceptAllPendingEdits() {
  for (PendingEdit &edit : m_pendingEdits) {
    edit.accepted = true;
  }

  emit pendingEditsChanged();
}

void EditSession::rejectAllPendingEdits() {
  for (PendingEdit &edit : m_pendingEdits) {
    edit.accepted = false;
  }

  emit pendingEditsChanged();
}

bool EditSession::applyPendingEdit(PendingEdit &edit) {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  if (!edit.completed) {
    emit failed(
        QStringLiteral("Pending edit %1 is not complete.").arg(edit.id));

    return false;
  }

  if (!edit.accepted) {
    return true;
  }

  if (edit.command.operation != EditCommand::Operation::Delete &&
      edit.command.newString.isEmpty()) {
    emit failed(QStringLiteral("Pending edit %1 has no generated content.")
                    .arg(edit.id));

    return false;
  }

  return m_applier->apply(*document, edit.command, edit.match);
}

bool EditSession::applyAcceptedPendingEdits() {
  if (!m_editor) {
    emit failed(noActiveEditorError());
    return false;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(invalidDocumentError());
    return false;
  }

  QVector<PendingEdit *> selected;

  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.completed && edit.accepted) {
      selected.append(&edit);
    }
  }

  std::sort(selected.begin(), selected.end(),
            [](const PendingEdit *left, const PendingEdit *right) {
              return left->match.start > right->match.start;
            });

  for (PendingEdit *edit : selected) {
    if (!applyPendingEdit(*edit)) {
      return false;
    }
  }

  document->rebuildStructure();

  m_pendingEdits.clear();

  emit pendingEditsChanged();
  setState(State::Idle);

  return true;
}

void EditSession::clearPendingEdits() {
  m_pendingEdits.clear();
  emit pendingEditsChanged();

  if (m_state == State::WaitingForReview) {
    setState(State::Idle);
  }
}

void EditSession::onCandidateSelected(int index) {
  if (m_state != State::AwaitingSelection) {
    return;
  }

  if (!m_editor) {
    abort();
    emit failed(noActiveEditorError());
    return;
  }

  TextDocument *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    abort();
    emit failed(invalidDocumentError());
    return;
  }

  if (document->revision() != m_documentRevision) {
    m_pendingCandidates.clear();
    setState(State::Idle);

    emit failed(QStringLiteral("Document changed before candidate selection."));

    return;
  }

  if (index < 0 || index >= m_pendingCandidates.size()) {
    emit failed(QStringLiteral("Invalid edit candidate."));
    return;
  }

  applyCandidate(m_pendingCandidates.at(index));
}