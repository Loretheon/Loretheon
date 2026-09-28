#pragma once

#include "EditCommand.h"
#include "EditMatch.h"
#include "EditMatcher.h"
#include "PendingEdit.h"

#include "../history/HistoryModel.h"
#include "inference/InferenceService.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QStringList>
#include <QVector>

class EditApplier;
class EditCandidateView;
class InferenceService;
class PayloadLogger;
class TextDocument;
class TextEdit;

class EditSession : public QObject {
  Q_OBJECT

public:
  enum class State {
    Idle,
    ValidatingPlan,
    AwaitingPlanApproval,
    Matching,
    AwaitingSelection,
    Streaming,
    WaitingForReview,
    Applying
  };
  void setSessionId(const QString &id) { m_sessionId = id; }
  Q_ENUM(State)

  // Normal mode: the editor is required and its document is the target.
  explicit EditSession(TextEdit *editor = nullptr, QObject *parent = nullptr);

  // Overseer mode: the document is the target and no editor is needed.
  // The document is owned by the caller and must outlive the session.
  // Factory because a document constructor would be ambiguous with the
  // editor constructor when both are given nullptr.
  static EditSession *forDocument(TextDocument *document,
                                  QObject *parent = nullptr);

  void setEditor(TextEdit *editor);
  void setInferenceService(InferenceService *service);

  TextDocument *document() const { return m_document; }

  State state() const { return m_state; }

  bool hasConflicts() const;

  bool validatePlan(const QJsonArray &planArray);
  bool executePlan(const QVector<EditCommand> &commands);
  bool resolvePlan(const QJsonArray &planArray);

  bool propose(const EditCommand &command);
  bool proposeMany(const QVector<EditCommand> &commands);

  bool prepareStreaming(const EditCommand &command, int pendingEditId);
  bool appendStreaming(const QString &text);
  bool finishStreaming();

  bool completeInsertFromInstruction(int pendingEditId);

  void startAllPendingEdits();

  bool allPendingEditsCompleted() const;

  void abort();

  const QVector<PendingEdit> &pendingEdits() const { return m_pendingEdits; }

  bool setPendingEditAccepted(int id, bool accepted);
  bool acceptPendingEdit(int id);
  bool rejectPendingEdit(int id);
  void acceptAllPendingEdits();
  void rejectAllPendingEdits();
  bool applyAcceptedPendingEdits();
  void clearPendingEdits();

  HistoryModel *historyModel() const { return m_historyModel; }

signals:
  void stateChanged(State state);

  void planValidated(const QVector<EditCommand> &commands);
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
  void pendingEditsApplied(const QStringList &touchedScopeIds);

  void generationFinished(bool allCompleted);

private slots:
  void onCandidateSelected(int index);

private:
  struct DocumentTag {};

  // Tagged constructor used by forDocument. Distinguished from the
  // editor constructor by the tag so overload resolution is
  // unambiguous.
  EditSession(DocumentTag, TextDocument *document, QObject *parent);

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

  PendingEdit *findPendingEdit(int id);

  void dispatchGeneration(PendingEdit &edit);

  QString systemPromptFor(const EditCommand &command) const;
  QString userPromptFor(const EditCommand &command) const;

  // Optional. Present in normal mode. Null in Overseer mode.
  TextEdit *m_editor = nullptr;

  // The target document. Always non-null after construction.
  TextDocument *m_document = nullptr;

  EditMatcher m_matcher;
  EditApplier *m_applier = nullptr;
  EditCandidateView *m_candidateView = nullptr;

  HistoryModel *m_historyModel = nullptr;

  InferenceService *m_inferenceService = nullptr;
  PayloadLogger *m_payloadLogger = nullptr;

  EditCommand m_pendingCommand;
  EditMatch m_pendingMatch;

  QVector<EditMatch> m_pendingCandidates;
  QVector<PendingEdit> m_pendingEdits;

  QHash<int, InferenceService::RequestToken> m_generationTokens;
  QHash<InferenceService::RequestToken, int> m_tokenToEditId;
  QHash<int, QString> m_generationBuffers;

  int m_documentRevision = -1;

  State m_state = State::Idle;
};