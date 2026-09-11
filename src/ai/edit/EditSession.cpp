#include "EditSession.h"

#include "EditApplier.h"
#include "EditCandidateView.h"

#include "../../text/TextEdit.h"
#include "../../text/model/TextDocument.h"

#include <QDebug>

EditSession::EditSession(TextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(editor) {
  m_applier = new EditApplier(this);
  m_candidateView = new EditCandidateView();

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
  for (const PendingEdit &edit : m_pendingEdits) {
    if (edit.hasConflict) {
      return true;
    }
  }
  return false;
}

void EditSession::detectConflicts() {
  for (int i = 0; i < m_pendingEdits.size(); ++i) {
    m_pendingEdits[i].hasConflict = false;
  }

  for (int i = 0; i < m_pendingEdits.size(); ++i) {
    for (int j = i + 1; j < m_pendingEdits.size(); ++j) {
      const EditMatch &m1 = m_pendingEdits[i].match;
      const EditMatch &m2 = m_pendingEdits[j].match;

      if (m1.isValid() && m2.isValid()) {
        const bool overlaps = (m1.start < m2.end && m1.end > m2.start);
        if (overlaps) {
          m_pendingEdits[i].hasConflict = true;
          m_pendingEdits[j].hasConflict = true;
        }
      }
    }
  }
}

bool EditSession::resolvePlan(const QJsonArray &planArray) {
  abort();

  if (!m_editor) {
    emit failed(QStringLiteral("No active editor."));
    return false;
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
    return false;
  }

  document->rebuildStructure();
  m_documentRevision = document->revision();

  QVector<EditCommand> commands;
  for (const QJsonValue &val : planArray) {
    if (val.isObject()) {
      EditCommand cmd = EditCommand::fromJson(val.toObject());
      if (cmd.isCommandValid()) {
        commands.append(cmd);
      }
    }
  }

  if (commands.isEmpty()) {
    emit failed(QStringLiteral("Plan contains no valid edit commands."));
    return false;
  }

  for (int i = 0; i < commands.size(); ++i) {
    EditMatch match;
    if (resolveCommand(commands[i], match)) {
      PendingEdit pending;
      pending.id = i + 1;
      pending.command = commands[i];
      pending.match = match;
      pending.accepted = true;
      pending.completed = false;
      m_pendingEdits.append(pending);
    }
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
    emit failed(QStringLiteral("No active editor."));
    return false;
  }

  if (!command.isCommandValid()) {
    emit failed(QStringLiteral("Invalid edit command."));
    return false;
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
    return false;
  }

  setState(State::Matching);
  document->rebuildStructure();
  m_documentRevision = document->revision();

  m_pendingCommand = command;
  m_pendingCandidates.clear();

  EditMatcher::Result result;

  if (command.operation == EditCommand::Operation::Insert) {
    EditMatch insertionMatch;
    if (!createInsertionMatch(command, insertionMatch)) {
      setState(State::Idle);
      return false;
    }
    result.candidates.append(insertionMatch);
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
    emit failed(QStringLiteral("No active editor."));
    return false;
  }

  if (commands.isEmpty()) {
    emit failed(QStringLiteral("Edit batch is empty."));
    return false;
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
    return false;
  }

  for (int i = 0; i < commands.size(); ++i) {
    if (!commands.at(i).isValid()) {
      emit failed(QStringLiteral("Edit %1 is invalid.").arg(i + 1));
      return false;
    }
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
    emit failed(QStringLiteral("No active editor."));
    return false;
  }

  if (!command.isCommandValid()) {
    emit failed(QStringLiteral("Invalid structural edit command."));
    return false;
  }

  if (m_applier && m_applier->isStreaming()) {
    m_applier->cancelStreaming();
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
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

  PendingEdit *pending = nullptr;

  for (PendingEdit &pe : m_pendingEdits) {
    if (pe.id == pendingEditId) {
      pending = &pe;
      break;
    }
  }

  if (!pending) {
    PendingEdit newEdit;
    newEdit.id = pendingEditId;
    newEdit.command = command;
    newEdit.match = match;
    newEdit.generatedText.clear();
    newEdit.accepted = true;
    newEdit.completed = false;

    m_pendingEdits.append(newEdit);

    pending = &m_pendingEdits.last();
  } else {
    pending->command = command;
    pending->match = match;
    pending->generatedText.clear();
    pending->accepted = true;
    pending->completed = false;
    pending->hasConflict = false;
  }

  emit pendingEditStarted(*pending);

  if (command.operation == EditCommand::Operation::Delete) {
    pending->completed = true;

    emit pendingEditFinished(*pending);

    m_pendingMatch = EditMatch();
    m_pendingCommand = EditCommand();
    m_currentPendingEditId = 0;

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
    emit failed(QStringLiteral("EditSession 1: No streaming edit is active."));
    return false;
  }


  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.id == m_currentPendingEditId) {
      edit.generatedText += text;
      emit pendingEditUpdated(edit);
      return true;
    }
  }

  emit failed(QStringLiteral("Pending edit was not found."));
  return false;
}

bool EditSession::finishStreaming() {
  if (m_state != State::Streaming) {
    emit failed(QStringLiteral("EditSession 2: No streaming edit is active."));
    return false;
  }



  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.id == m_currentPendingEditId) {
      if (edit.command.operation != EditCommand::Operation::Delete &&
          edit.generatedText.isEmpty()) {
        emit failed(QStringLiteral("Streaming edit produced no content."));
        return false;
      }

      edit.command.newString = edit.generatedText;
      edit.completed = true;

      emit pendingEditFinished(edit);
      break;
    }
  }

  m_pendingMatch = EditMatch();
  m_pendingCommand = EditCommand();
  m_currentPendingEditId = 0;

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
  m_currentPendingEditId = 0;
  m_documentRevision = -1;

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
    emit failed(QStringLiteral("No active editor."));
    return;
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    setState(State::Idle);
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
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
    emit failed(QStringLiteral("No active editor."));
    return false;
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
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
  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
    return false;
  }

  matches.clear();
  matches.reserve(commands.size());

  for (int i = 0; i < commands.size(); ++i) {
    EditMatch match;
    if (!resolveSingle(commands.at(i), match)) {
      emit failed(QStringLiteral("Edit %1 could not be resolved.").arg(i + 1));
      return false;
    }
    matches.append(match);
  }

  if (document->revision() != m_documentRevision) {
    emit failed(QStringLiteral("Document changed while resolving edits."));
    return false;
  }

  return true;
}

bool EditSession::createInsertionMatch(const EditCommand &command,
                                       EditMatch &match) {
  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
    return false;
  }

  if (!command.findString.isEmpty()) {
    emit failed(
        QStringLiteral("Insert operations must have an empty find string."));
    return false;
  }

  document->rebuildStructure();
  const DocumentStructure &structure = document->structure();
  const DocumentNode *scope = structure.find(command.scopeId);

  if (!scope) {
    emit failed(QStringLiteral("Insertion scope '%1' was not found.")
                    .arg(command.scopeId));
    return false;
  }

  const int scopeStart = scope->start;
  const int scopeEnd = scope->end;
  const int documentLength = document->toPlainText().size();

  const bool validStart = scopeStart >= 0 && scopeStart <= documentLength;
  const bool validEnd = scopeEnd >= scopeStart && scopeEnd <= documentLength;

  if (!validStart || !validEnd) {
    emit failed(QStringLiteral("Insertion scope '%1' has an invalid range.")
                    .arg(command.scopeId));
    return false;
  }

  int position = scopeStart;
  if (command.position == EditCommand::Position::After) {
    position = scopeEnd;
  }

  match.start = position;
  match.end = position;
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
    emit failed(QStringLiteral("No active editor."));
    return false;
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
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
    emit failed(QStringLiteral("No active editor."));
    return false;
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
    return false;
  }

  QVector<PendingEdit *> selected;
  for (PendingEdit &edit : m_pendingEdits) {
    if (edit.completed && edit.accepted) {
      selected.append(&edit);
    }
  }

  std::sort(selected.begin(), selected.end(),
            [](const PendingEdit *a, const PendingEdit *b) {
              return a->match.start > b->match.start;
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
    emit failed(QStringLiteral("No active editor."));
    return;
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());
  if (!document) {
    abort();
    emit failed(QStringLiteral("Active editor does not use TextDocument."));
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

  const EditMatch match = m_pendingCandidates.at(index);
  applyCandidate(match);
}