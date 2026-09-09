#pragma once

#include "EditCommand.h"
#include "EditMatch.h"
#include "EditMatcher.h"

#include <QObject>
#include <QVector>
#include <optional>

class EditApplier;
class EditCandidateView;
class TextEdit;

class EditSession : public QObject {
  Q_OBJECT

public:
  enum class State { Idle, Matching, AwaitingSelection, Applying };
  Q_ENUM(State)

  explicit EditSession(TextEdit *editor = nullptr, QObject *parent = nullptr);

  void setEditor(TextEdit *editor);

  State state() const { return m_state; }

  bool propose(const EditCommand &command);

  void abort();

signals:
  void stateChanged(State state);

  void candidatesReady(const QVector<EditMatch> &candidates, bool fuzzy);

  void applied(bool fuzzy, int editDistance);

  void failed(const QString &reason);

  void aborted();

private slots:
  void onCandidateSelected(int index);

private:
  void setState(State state);

  void applyCandidate(const EditMatch &match);

  TextEdit *m_editor{nullptr};

  EditMatcher m_matcher;
  EditApplier *m_applier{nullptr};
  EditCandidateView *m_candidateView{nullptr};

  std::optional<EditCommand> m_pendingCommand;
  QVector<EditMatch> m_pendingCandidates;

  int m_documentRevision{-1};

  State m_state{State::Idle};
};