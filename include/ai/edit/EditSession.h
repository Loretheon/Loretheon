#pragma once

#include "EditCommand.h"
#include "EditMatch.h"
#include "EditMatcher.h"
#include "PendingEdit.h"

#include <QJsonArray>
#include <QObject>
#include <QVector>

class EditApplier;
class EditCandidateView;
class TextEdit;

class EditSession : public QObject {
  Q_OBJECT

public:
  enum class State {
    Idle,
    Matching,
    AwaitingSelection,
    Streaming,
    WaitingForReview,
    Applying
  };
  void setSessionId(const QString &id) { m_sessionId = id; }
  Q_ENUM(State)

  explicit EditSession(TextEdit *editor = nullptr, QObject *parent = nullptr);

  void setEditor(TextEdit *editor);

  State state() const { return m_state; }

  bool hasConflicts() const;

  bool resolvePlan(const QJsonArray &planArray);

  bool propose(const EditCommand &command);

  bool proposeMany(const QVector<EditCommand> &commands);

  bool prepareStreaming(const EditCommand &command, int pendingEditId);

  bool appendStreaming(const QString &text);

  bool finishStreaming();

  void abort();

  const QVector<PendingEdit> &pendingEdits() const { return m_pendingEdits; }

  bool setPendingEditAccepted(int id, bool accepted);

  void acceptAllPendingEdits();

  void rejectAllPendingEdits();

  bool applyAcceptedPendingEdits();

  void clearPendingEdits();

signals:
  void stateChanged(State state);

  void candidatesReady(const QVector<EditMatch> &candidates);

  void applied(bool fuzzy, int editDistance);

  void failed(const QString &reason);

  void aborted();

  void pendingEditStarted(const PendingEdit &edit);

  void pendingEditUpdated(const PendingEdit &edit);

  void pendingEditFinished(const PendingEdit &edit);

  void pendingEditsChanged();

  void reviewReady();

  void conflictsDetected();

  void planReady(const QVector<EditCommand> &plannedCommands);

private slots:
  void onCandidateSelected(int index);

private:

  QString m_sessionId;
  void setState(State state);

  void applyCandidate(const EditMatch &match);

  bool resolveSingle(const EditCommand &command, EditMatch &match);

  bool resolveBatch(const QVector<EditCommand> &commands,
                    QVector<EditMatch> &matches);

  bool createInsertionMatch(const EditCommand &command, EditMatch &match);

  bool resolveCommand(const EditCommand &command, EditMatch &match);

  bool applyPendingEdit(PendingEdit &edit);

  void detectConflicts();

  TextEdit *m_editor = nullptr;
  EditMatcher m_matcher;
  EditApplier *m_applier = nullptr;
  EditCandidateView *m_candidateView = nullptr;

  EditCommand m_pendingCommand;
  EditMatch m_pendingMatch;

  QVector<EditMatch> m_pendingCandidates;
  QVector<PendingEdit> m_pendingEdits;

  int m_documentRevision = -1;
  int m_currentPendingEditId = 0;

  State m_state = State::Idle;
};