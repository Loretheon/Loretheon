#pragma once

#include "EditCommand.h"
#include "EditMatch.h"
#include "EditMatcher.h"

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
    Applying
};

  Q_ENUM(State)

  explicit EditSession(
      TextEdit *editor = nullptr,
      QObject *parent = nullptr);

  void setEditor(
      TextEdit *editor);

  State state() const {
    return m_state;
  }

  bool propose(
      const EditCommand &command);

  bool proposeMany(
      const QVector<EditCommand> &commands);

  void abort();

  signals:
      void stateChanged(
          State state);

  void candidatesReady(
      const QVector<EditMatch> &candidates);

  void applied(
      bool fuzzy,
      int editDistance);

  void failed(
      const QString &reason);

  void aborted();

private slots:
    void onCandidateSelected(
        int index);

private:
  void setState(
      State state);

  void applyCandidate(
      const EditMatch &match);

  bool resolveSingle(
      const EditCommand &command,
      EditMatch &match);

  bool resolveBatch(
      const QVector<EditCommand> &commands,
      QVector<EditMatch> &matches);

  bool createInsertionMatch(
      const EditCommand &command,
      EditMatch &match);

  TextEdit *m_editor = nullptr;

  EditMatcher m_matcher;

  EditApplier *m_applier = nullptr;

  EditCandidateView *m_candidateView = nullptr;

  EditCommand m_pendingCommand;

  QVector<EditMatch> m_pendingCandidates;

  int m_documentRevision = -1;

  State m_state = State::Idle;
};