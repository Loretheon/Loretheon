#include "EditSession.h"

#include "EditApplier.h"
#include "EditCandidateView.h"

#include "../../text/TextEdit.h"

#include <QTextDocument>

EditSession::EditSession(TextEdit *editor, QObject *parent)
    : QObject(parent), m_editor(editor), m_applier(new EditApplier(this)),
      m_candidateView(new EditCandidateView(editor, this)) {
  connect(m_candidateView, &EditCandidateView::candidateSelected, this,
          &EditSession::onCandidateSelected);

  connect(m_applier, &EditApplier::applied, this, &EditSession::applied);

  connect(m_applier, &EditApplier::failed, this, &EditSession::failed);

  if (m_editor) {
    m_documentRevision = m_editor->document()->revision();
  }
}

void EditSession::setEditor(TextEdit *editor) {
  if (m_candidateView) {
    m_candidateView->setEditor(editor);
  }

  m_editor = editor;

  m_pendingCommand.reset();
  m_pendingCandidates.clear();

  if (m_editor) {
    m_documentRevision = m_editor->document()->revision();
  } else {
    m_documentRevision = -1;
  }

  setState(State::Idle);
}

bool EditSession::propose(const EditCommand &command) {
  abort();

  if (!m_editor) {
    emit failed(QStringLiteral("No editor attached"));
    return false;
  }

  if (!command.isValid()) {
    emit failed(QStringLiteral(
        "old_string and new_string must be non-empty and different"));
    return false;
  }

  m_pendingCommand = command;

  m_documentRevision = m_editor->document()->revision();

  setState(State::Matching);

  const EditMatcher::Result result =
      m_matcher.find(*m_editor->document(), command);

  /*
   * The document could theoretically change between taking the
   * revision snapshot and completing matching. Do not use stale
   * offsets.
   */
  if (m_editor->document()->revision() != m_documentRevision) {
    m_pendingCommand.reset();

    setState(State::Idle);

    emit failed(QStringLiteral("Document changed while locating the edit"));

    return false;
  }

  if (result.candidates.isEmpty()) {
    m_pendingCommand.reset();

    setState(State::Idle);

    emit failed(QStringLiteral("No matching text found"));

    return false;
  }

  if (result.candidates.size() == 1) {
    applyCandidate(result.candidates.first());

    return true;
  }

  /*
   * Multiple candidates are never resolved automatically.
   *
   * The user chooses the intended occurrence.
   */
  m_pendingCandidates = result.candidates;

  m_candidateView->showCandidates(m_pendingCandidates);

  setState(State::AwaitingSelection);

  emit candidatesReady(m_pendingCandidates, result.fuzzy);

  return true;
}

void EditSession::abort() {
  const bool wasActive = m_state != State::Idle || m_pendingCommand.has_value();

  m_pendingCommand.reset();
  m_pendingCandidates.clear();

  if (m_candidateView) {
    m_candidateView->clear();
  }

  setState(State::Idle);

  if (wasActive) {
    emit aborted();
  }
}

void EditSession::onCandidateSelected(int index) {
  if (m_state != State::AwaitingSelection) {
    return;
  }

  if (index < 0 || index >= m_pendingCandidates.size()) {
    return;
  }

  if (!m_pendingCommand.has_value()) {
    abort();

    emit failed(QStringLiteral("No pending edit"));

    return;
  }

  if (!m_editor || m_editor->document()->revision() != m_documentRevision) {
    abort();

    emit failed(
        QStringLiteral("Document changed before the edit was selected"));

    return;
  }

  const EditMatch match = m_pendingCandidates[index];

  applyCandidate(match);
}

void EditSession::applyCandidate(const EditMatch &match) {
  if (!m_pendingCommand.has_value()) {
    emit failed(QStringLiteral("No pending edit command"));
    return;
  }

  if (!m_editor) {
    emit failed(QStringLiteral("No editor attached"));
    return;
  }

  if (m_editor->document()->revision() != m_documentRevision) {
    abort();

    emit failed(QStringLiteral("Document changed before the edit was applied"));

    return;
  }

  setState(State::Applying);

  m_candidateView->clear();

  const EditCommand command = *m_pendingCommand;

  m_pendingCommand.reset();
  m_pendingCandidates.clear();

  m_applier->apply(*m_editor->document(), command, match);

  setState(State::Idle);
}

void EditSession::setState(State state) {
  if (m_state == state) {
    return;
  }

  m_state = state;

  emit stateChanged(m_state);
}