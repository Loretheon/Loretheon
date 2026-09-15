#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include "PayloadLogger.h"

#include "../context/ContextModel.h"
#include "../edit/EditCommand.h"
#include "../edit/EditMatch.h"
#include "../edit/PendingEdit.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class NotificationManager;

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QVBoxLayout;

class ChatWidgetEditFlow;
class ContextPanel;
class HistoryPanel;

class EditPlanner;
class EditSession;
class EditSessionWidget;

class InferenceService;

class TextEdit;

class ChatWidget : public QWidget {
  Q_OBJECT

  friend class ChatWidgetEditFlow;

public:
  explicit ChatWidget(InferenceService *inferenceService,
                      EditSession *editSession, QWidget *parent = nullptr);

  void setActiveEditor(TextEdit *editor);

  void submitTranscribedText(const QString &text);

  ContextModel *contextModel() const { return m_contextModel; }

  void triggerWholeFileRewrite();

signals:
  void contextScopesChanged(const QStringList &scopeIds);

  void previewActivationRequested(bool active);

private slots:
  void onPlanValidatedFromSession(const QVector<EditCommand> &commands);
  void onSendClicked();

  void onLlmDelta(const QString &text);

  void onLlmFinished();

  void onLlmError(const QString &error);

  void onPendingEditStarted(const PendingEdit &edit);

  void onPendingEditUpdated(const PendingEdit &edit);

  void onPendingEditFinished(const PendingEdit &edit);

  void onPlanValidated(const QVector<EditCommand> &commands);

  void onPlanFailed(const QString &reason);

  void onPlanApprovalRequested(const QVector<EditCommand> &commands);

  void onPlanCancelled();

  void onEditCandidatesReady(const QVector<EditMatch> &candidates);

  void onEditApplied(bool fuzzy, int editDistance);

  void onEditFailed(const QString &reason);

  void onEditAborted();

  void onConflictsDetected();

  void onPlanReady(const QVector<EditCommand> &plannedCommands);

  void onConflictResolved(int groupId, int keepEditNumber);

  void onConflictGroupDiscarded(int groupId);

  void onConflictBatchAborted();

  void onPendingEditAccepted(int editNumber);

  void onPendingEditRejected(int editNumber);

  void onAcceptAllPendingEdits();

  void onRejectAllPendingEdits();

  void onApplyAcceptedPendingEdits();

  void onReviewReady();

  void onDocumentStructureChanged();

private:
  enum class EditPhase { None, PlanReview, Content };

  void sendPrompt(const QString &prompt);

  void sendPromptWithMode(const QString &prompt, int scopeMode);

  void appendUserMessage(const QString &text);

  void appendAssistantChunk(const QString &text);

  void appendStatusMessage(const QString &text);

  void renderLastAssistantMessage();

  void resetEditState();

  void refreshContextModel();

  QStringList expandSentScopes(const QStringList &scopeIds) const;

private:
  NotificationManager *m_notifications = nullptr;

  ChatWidgetEditFlow *m_editFlow = nullptr;

  InferenceService *m_inferenceService = nullptr;

  EditSession *m_editSession = nullptr;

  EditPlanner *m_editPlanner = nullptr;

  TextEdit *m_activeEditor = nullptr;

  PayloadLogger m_payloadLogger;

  QTextEdit *m_transcript = nullptr;

  QLineEdit *m_input = nullptr;

  QPushButton *m_sendButton = nullptr;

  QCheckBox *m_editModeCheckbox = nullptr;

  QComboBox *m_editModeCombo = nullptr;

  EditSessionWidget *m_editSessionWidget = nullptr;

  ContextPanel *m_contextPanel = nullptr;

  HistoryPanel *m_historyPanel = nullptr;

  ContextModel *m_contextModel = nullptr;

  QVBoxLayout *m_layout = nullptr;

  QString m_currentEditRequest;

  QString m_currentCommandJson;

  QString m_currentCommandDescription;

  QString m_currentEditInstruction;

  QString m_currentLlmResponse;

  QList<EditCommand> m_plannedEdits;

  EditPhase m_editPhase = EditPhase::None;

  size_t m_nextPlannedEditIndex = 0;

  int m_currentEditNumber = 0;

  int m_streamingEditCount = 0;

  bool m_assistantMessageOpen = false;

  bool m_awaitingEdit = false;

  bool m_editGenerationStopped = false;

  bool m_editAbortRequested = false;

  bool m_planReadyToStream = false;
};

#endif // CHATWIDGET_H