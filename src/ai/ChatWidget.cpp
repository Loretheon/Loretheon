#include "ChatWidget.h"

#include "EditGrammar.h"
#include "EditSessionWidget.h"
#include "NotificationManager.h"
#include "edit/EditPlanner.h"
#include "edit/EditSession.h"

#include "../text/TextEdit.h"

#include "inference/InferenceService.h"

#include <QCheckBox>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QSplitter>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFormat>
#include <QTextFrameFormat>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

constexpr int TranscriptOuterMargin = 14;
constexpr int TranscriptMessageSpacing = 10;
constexpr int TranscriptStatusSpacing = 8;
constexpr int TranscriptInputHeight = 38;
constexpr int TranscriptSendWidth = 76;

QString editCommandOperationString(const EditCommand &command) {
  switch (command.operation) {
  case EditCommand::Operation::Insert:
    return QStringLiteral("insert");

  case EditCommand::Operation::Replace:
    return QStringLiteral("replace");

  case EditCommand::Operation::Delete:
    return QStringLiteral("delete");
  }

  return {};
}

QString editCommandPositionString(const EditCommand &command) {
  switch (command.position) {
  case EditCommand::Position::Before:
    return QStringLiteral("before");

  case EditCommand::Position::After:
    return QStringLiteral("after");
  }

  return {};
}

QJsonObject editCommandToJson(const EditCommand &command) {
  QJsonObject object;

  object.insert(QStringLiteral("operation"),
                editCommandOperationString(command));

  object.insert(QStringLiteral("scope"), command.scopeId);

  object.insert(QStringLiteral("position"), editCommandPositionString(command));

  object.insert(QStringLiteral("find"), command.findString);

  object.insert(QStringLiteral("all"), command.replaceAll);

  object.insert(QStringLiteral("instruction"), command.instruction);

  return object;
}

QTextBlockFormat
messageBlockFormat(int topMargin = 0,
                   int bottomMargin = TranscriptMessageSpacing) {
  QTextBlockFormat format;
  format.setTopMargin(topMargin);
  format.setBottomMargin(bottomMargin);
  format.setLineHeight(110, QTextBlockFormat::ProportionalHeight);

  return format;
}

QTextBlockFormat statusBlockFormat() {
  QTextBlockFormat format =
      messageBlockFormat(TranscriptStatusSpacing, TranscriptStatusSpacing);

  format.setLeftMargin(4);
  format.setRightMargin(4);

  return format;
}

QTextCharFormat senderFormat() {
  QTextCharFormat format;
  format.setFontWeight(QFont::DemiBold);

  return format;
}

QTextCharFormat statusFormat() {
  QTextCharFormat format;
  format.setFontItalic(true);

  return format;
}

void appendMessageSeparator(QTextCursor &cursor) {
  QTextCharFormat separatorFormat;
  separatorFormat.setForeground(cursor.document()->defaultStyleSheet().isEmpty()
                                    ? QBrush(cursor.charFormat().foreground())
                                    : QBrush(cursor.charFormat().foreground()));

  QTextBlockFormat format;
  format.setTopMargin(0);
  format.setBottomMargin(0);
  format.setLeftMargin(0);
  format.setRightMargin(0);

  cursor.insertBlock(format);
}

struct ChatWidgetLayoutBuilder {
  QTextEdit *transcript = nullptr;
  QLineEdit *input = nullptr;
  QPushButton *sendButton = nullptr;
  QCheckBox *editModeCheckbox = nullptr;
  EditSessionWidget *editSessionWidget = nullptr;
  QSplitter *contentSplitter = nullptr;
  QVBoxLayout *rootLayout = nullptr;

  void build(QWidget *parent) {
    transcript = new QTextEdit(parent);
    transcript->setReadOnly(true);
    transcript->setAcceptRichText(true);
    transcript->setLineWrapMode(QTextEdit::WidgetWidth);
    transcript->setFrameShape(QFrame::NoFrame);

    QPalette palette = transcript->palette();
    transcript->setTextBackgroundColor(palette.color(QPalette::Base));

    QTextDocument *document = transcript->document();
    document->setDocumentMargin(TranscriptOuterMargin);

    input = new QLineEdit(parent);
    input->setPlaceholderText(QObject::tr("Ask the assistant…"));
    input->setClearButtonEnabled(true);
    input->setMinimumHeight(TranscriptInputHeight);

    sendButton = new QPushButton(QObject::tr("Send"), parent);

    sendButton->setMinimumHeight(TranscriptInputHeight);

    sendButton->setMinimumWidth(TranscriptSendWidth);

    sendButton->setDefault(true);

    editModeCheckbox = new QCheckBox(QObject::tr("Edit document"), parent);

    editModeCheckbox->setToolTip(
        QObject::tr("Plan and preview document edits before applying them."));

    editSessionWidget = new EditSessionWidget(parent);

    contentSplitter = new QSplitter(Qt::Horizontal, parent);

    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->setHandleWidth(6);
    contentSplitter->addWidget(transcript);
    contentSplitter->addWidget(editSessionWidget);
    contentSplitter->setSizes({680, 420});
    contentSplitter->setStretchFactor(0, 1);
    contentSplitter->setStretchFactor(1, 0);

    auto *controlsFrame = new QWidget(parent);

    auto *controlsLayout = new QHBoxLayout(controlsFrame);

    controlsLayout->setContentsMargins(10, 8, 10, 10);

    controlsLayout->setSpacing(8);

    controlsLayout->addWidget(editModeCheckbox);

    controlsLayout->addWidget(input, 1);

    controlsLayout->addWidget(sendButton);

    rootLayout = new QVBoxLayout(parent);

    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(contentSplitter, 1);
    rootLayout->addWidget(controlsFrame);
  }
};

} // namespace

ChatWidget::ChatWidget(InferenceService *inferenceService,
                       EditSession *editSession, QWidget *parent)
    : QWidget(parent), m_inferenceService(inferenceService),
      m_editSession(editSession) {
  m_notifications = new NotificationManager(this);

  ChatWidgetLayoutBuilder layout;
  layout.build(this);

  m_transcript = layout.transcript;
  m_input = layout.input;
  m_sendButton = layout.sendButton;
  m_editModeCheckbox = layout.editModeCheckbox;
  m_editSessionWidget = layout.editSessionWidget;
  m_layout = layout.rootLayout;

  setLayout(m_layout);

  if (m_inferenceService) {
    m_editPlanner = new EditPlanner(m_inferenceService, this);

    connect(m_editPlanner, &EditPlanner::planReady, this,
            &ChatWidget::onPlanReady);

    connect(m_editPlanner, &EditPlanner::failed, this,
            &ChatWidget::onPlanFailed);
  }

  connect(m_sendButton, &QPushButton::clicked, this,
          &ChatWidget::onSendClicked);

  connect(m_input, &QLineEdit::returnPressed, this, &ChatWidget::onSendClicked);

  if (m_inferenceService) {
    connect(m_inferenceService, &InferenceService::llmDelta, this,
            &ChatWidget::onLlmDelta);

    connect(m_inferenceService, &InferenceService::llmFinished, this,
            &ChatWidget::onLlmFinished);

    connect(m_inferenceService, &InferenceService::llmError, this,
            &ChatWidget::onLlmError);
  }

  if (m_editSession) {
    connect(m_editSession, &EditSession::candidatesReady, this,
            &ChatWidget::onEditCandidatesReady);

    connect(m_editSession, &EditSession::applied, this,
            &ChatWidget::onEditApplied);

    connect(m_editSession, &EditSession::failed, this,
            &ChatWidget::onEditFailed);

    connect(m_editSession, &EditSession::aborted, this,
            &ChatWidget::onEditAborted);

    connect(m_editSession, &EditSession::conflictsDetected, this,
            &ChatWidget::onConflictsDetected);

    connect(m_editSession, &EditSession::planReady, this,
            &ChatWidget::onPlanReady);

    connect(m_editSession, &EditSession::reviewReady, this,
            &ChatWidget::onReviewReady);

    connect(m_editSession, &EditSession::pendingEditStarted, this,
            [this](const PendingEdit &edit) {
              if (m_activeEditor) {
                m_activeEditor->showPendingEdit(edit);
              }
            });

    connect(m_editSession, &EditSession::pendingEditUpdated, this,
            [this](const PendingEdit &edit) {
              if (m_activeEditor) {
                m_activeEditor->updatePendingEdit(edit);
              }
            });

    connect(m_editSession, &EditSession::pendingEditFinished, this,
            [this](const PendingEdit &edit) {
              if (edit.id <= 0) {
                return;
              }

              m_editSessionWidget->setPendingEditReady(edit.id);

              if (m_activeEditor && m_activeEditor->autoAcceptEdits()) {
                onPendingEditAccepted(edit.id);
              }

              if (!m_awaitingEdit || m_editPhase == EditPhase::None) {
                return;
              }

              if (edit.id != m_currentEditNumber) {
                return;
              }

              ++m_nextPlannedEditIndex;

              m_currentEditNumber = 0;
              m_currentEditInstruction.clear();
              m_currentCommandDescription.clear();

              if (m_nextPlannedEditIndex >=
                  static_cast<size_t>(m_plannedEdits.size())) {
                m_editPhase = EditPhase::None;
                m_editGenerationStopped = false;
                m_editAbortRequested = false;

                m_notifications->notify(
                    tr("Edits ready"),
                    tr("The generated edits are ready for review."));

                return;
              }

              m_editGenerationStopped = false;
              m_editAbortRequested = false;

              QMetaObject::invokeMethod(
                  this,
                  [this]() {
                    if (!m_awaitingEdit || m_editPhase == EditPhase::None) {
                      return;
                    }

                    executeNextPlannedEdit();
                  },
                  Qt::QueuedConnection);
            });

    connect(m_editSession, &EditSession::pendingEditsChanged, this, [this]() {
      if (m_activeEditor) {
        m_activeEditor->refreshPendingEdits();
      }
    });
  }

  connect(m_editSessionWidget, &EditSessionWidget::pendingEditAccepted, this,
          &ChatWidget::onPendingEditAccepted);

  connect(m_editSessionWidget, &EditSessionWidget::pendingEditRejected, this,
          &ChatWidget::onPendingEditRejected);

  connect(m_editSessionWidget,
          &EditSessionWidget::acceptAllPendingEditsRequested, this,
          &ChatWidget::onAcceptAllPendingEdits);

  connect(m_editSessionWidget,
          &EditSessionWidget::rejectAllPendingEditsRequested, this,
          &ChatWidget::onRejectAllPendingEdits);

  connect(m_editSessionWidget,
          &EditSessionWidget::applyAcceptedPendingEditsRequested, this,
          &ChatWidget::onApplyAcceptedPendingEdits);

  connect(m_editSessionWidget, &EditSessionWidget::skipRequested, this,
          [this]() {
            if (m_editPlanner) {
              m_editPlanner->abort();
            }

            if (m_inferenceService) {
              m_editGenerationStopped = true;
              m_editAbortRequested = true;
              m_inferenceService->abortChatRequest();
            }

            if (m_editSession) {
              m_editSession->abort();
            }

            resetEditState();
          });

  connect(m_editSessionWidget, &EditSessionWidget::conflictResolved, this,
          &ChatWidget::onConflictResolved);

  connect(m_editSessionWidget, &EditSessionWidget::conflictGroupDiscarded, this,
          &ChatWidget::onConflictGroupDiscarded);

  connect(m_editSessionWidget, &EditSessionWidget::conflictBatchAborted, this,
          &ChatWidget::onConflictBatchAborted);
}

void ChatWidget::setActiveEditor(TextEdit *editor) {
  m_activeEditor = editor;

  if (m_editSession) {
    m_editSession->setEditor(editor);
  }

  if (m_activeEditor && m_editSession) {
    m_activeEditor->refreshPendingEdits();
  }
}

void ChatWidget::submitTranscribedText(const QString &text) {
  if (text.trimmed().isEmpty()) {
    return;
  }

  m_input->setText(text);
  sendPrompt(text);
}

void ChatWidget::onSendClicked() {
  const QString prompt = m_input->text().trimmed();

  if (prompt.isEmpty()) {
    return;
  }

  m_input->clear();
  sendPrompt(prompt);
}

void ChatWidget::sendPrompt(const QString &prompt) {
  if (!m_inferenceService) {
    appendStatusMessage(tr("Inference service is unavailable."));
    return;
  }

  if (m_editPlanner) {
    m_editPlanner->abort();
  }

  if (m_editSession) {
    m_editSession->abort();
  }

  m_inferenceService->abortChatRequest();

  resetEditState();

  m_currentLlmResponse.clear();
  m_currentEditRequest = prompt;

  appendUserMessage(prompt);

  m_payloadLogger.log(QStringLiteral("USER_PROMPT"),
                      QStringLiteral("Prompt: \"%1\"").arg(prompt));

  const bool editMode = m_editModeCheckbox && m_editModeCheckbox->isChecked();

  if (editMode) {
    m_awaitingEdit = true;

    if (!m_activeEditor) {
      appendStatusMessage(tr("No active document."));
      resetEditState();
      return;
    }

    if (!m_editPlanner) {
      appendStatusMessage(tr("Edit planner is unavailable."));
      resetEditState();
      return;
    }

    m_editSessionWidget->clearHistory();
    requestNextEditCommand();
    return;
  }

  QJsonArray messages;

  messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                              {QStringLiteral("content"), prompt}});

  m_payloadLogger.log(QStringLiteral("CHAT_REQUEST"),
                      QStringLiteral("Prompt: \"%1\"").arg(prompt));

  m_inferenceService->sendChatRequest(messages, QString(), 0.7, 120000);
}

void ChatWidget::requestNextEditCommand() {
  if (!m_editPlanner || !m_activeEditor || !m_awaitingEdit) {
    return;
  }

  appendStatusMessage(tr("Planning edits…"));

  m_editPlanner->start(m_activeEditor, m_currentEditRequest);
}

void ChatWidget::beginStreamingResolvedPlan() {
  if (!m_awaitingEdit || !m_planReadyToStream) {
    return;
  }

  executeNextPlannedEdit();
}

void ChatWidget::executeNextPlannedEdit() {
  if (!m_awaitingEdit || !m_planReadyToStream) {
    return;
  }

  if (m_nextPlannedEditIndex >= static_cast<size_t>(m_plannedEdits.size())) {
    m_editPhase = EditPhase::None;
    m_awaitingEdit = false;

    appendStatusMessage(tr("All planned edits are ready for review."));

    m_notifications->notify(tr("Edits ready"),
                            tr("The generated edits are ready for review."));

    return;
  }

  m_currentEditNumber = static_cast<int>(m_nextPlannedEditIndex + 1);

  const EditCommand command =
      m_plannedEdits.at(static_cast<int>(m_nextPlannedEditIndex));

  m_currentEditInstruction = command.instruction;

  m_currentCommandDescription = describeCommand(command);

  m_editSessionWidget->setStatus(m_currentEditNumber, tr("Resolving"));

  appendStatusMessage(
      tr("Edit %1 · resolving target…").arg(m_currentEditNumber));

  if (!m_editSession->prepareStreaming(command, m_currentEditNumber)) {
    m_editSessionWidget->setStatus(m_currentEditNumber, tr("Failed"));

    m_editSessionWidget->failEdit(
        m_currentEditNumber, tr("Unable to resolve the requested target."));

    resetEditState();
    return;
  }

  if (command.operation == EditCommand::Operation::Delete) {
    return;
  }

  m_editSessionWidget->setStatus(m_currentEditNumber, tr("Writing"));

  requestEditContent();
}

void ChatWidget::requestEditContent() {
  if (!m_inferenceService || !m_activeEditor || !m_awaitingEdit) {
    return;
  }

  QString cleanInstruction = m_currentEditInstruction.trimmed();

  QString cleanDescription = m_currentCommandDescription.trimmed();

  if (cleanDescription.startsWith('[') && cleanDescription.endsWith(']')) {
    cleanDescription.remove(0, 1);
    cleanDescription.chop(1);
    cleanDescription = cleanDescription.trimmed();
  }

  QJsonArray messages;

  const QString systemPrompt =
      QStringLiteral("You are an automated document text generator. "
                     "You output raw Markdown text directly. "
                     "Never output JSON, punctuation wrappers, or "
                     "closing brackets like ']'. ");

  const QString contentPrompt =
      QStringLiteral("TASK: Generate the replacement Markdown "
                     "content for a single document edit.\n\n"
                     "INSTRUCTIONS:\n"
                     "- Output ONLY the final text/paragraph to "
                     "insert or replace.\n"
                     "- Do not write JSON.\n"
                     "- Do not write explanations.\n"
                     "- Do not output leading closing brackets "
                     "or syntax symbols.\n\n"
                     "EDIT INSTRUCTION:\n%1\n\n"
                     "TARGET DETAILS:\n%2\n\n"
                     "BEGIN MARKDOWN CONTENT:")
          .arg(cleanInstruction, cleanDescription);

  messages.append(
      QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                  {QStringLiteral("content"), systemPrompt}});

  messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                              {QStringLiteral("content"), contentPrompt}});

  m_editPhase = EditPhase::Content;
  m_currentLlmResponse.clear();

  const QString currentScopeId =
      (m_currentEditNumber > 0 && m_currentEditNumber <= m_plannedEdits.size())
          ? m_plannedEdits.at(m_currentEditNumber - 1).scopeId
          : QStringLiteral("unknown");

  m_payloadLogger.log(
      QStringLiteral("MARKDOWN_GEN_REQUEST"),
      QStringLiteral("Edit #%1 | Scope: %2 | Instruction: \"%3\"")
          .arg(QString::number(m_currentEditNumber), currentScopeId,
               cleanInstruction));

  m_inferenceService->sendChatRequest(messages, QString(), 0.7, 120000);
}

void ChatWidget::onLlmDelta(const QString &text) {
  if (text.isEmpty()) {
    return;
  }

  m_currentLlmResponse += text;

  if (m_awaitingEdit) {
    if (m_editPhase != EditPhase::Content || m_editGenerationStopped) {
      return;
    }

    if (m_editSession) {
      m_editSession->appendStreaming(text);
    }

    return;
  }

  appendAssistantChunk(text);
}

void ChatWidget::onLlmFinished() {
  if (m_awaitingEdit && m_editPhase == EditPhase::Content) {
    m_payloadLogger.log(
        QStringLiteral("MARKDOWN_GEN_RESPONSE"),
        QStringLiteral("Edit #%1\n%2")
            .arg(QString::number(m_currentEditNumber), m_currentLlmResponse));
  } else if (!m_currentLlmResponse.isEmpty()) {
    m_payloadLogger.log(QStringLiteral("CHAT_RESPONSE"), m_currentLlmResponse);
  }

  m_currentLlmResponse.clear();

  if (!m_awaitingEdit) {
    m_assistantMessageOpen = false;
    return;
  }

  if (m_editAbortRequested) {
    return;
  }

  if (m_editPhase == EditPhase::Content && m_editSession) {
    m_editSession->finishStreaming();
  }
}

void ChatWidget::onLlmError(const QString &error) {
  m_payloadLogger.log(QStringLiteral("MARKDOWN_GEN_ERROR"),
                      QStringLiteral("Edit #%1 Error: %2")
                          .arg(QString::number(m_currentEditNumber), error));

  m_currentLlmResponse.clear();

  if (m_awaitingEdit && m_editPhase != EditPhase::Content) {
    return;
  }

  if (m_editAbortRequested) {
    return;
  }

  if (m_notifications) {
    m_notifications->notify(
        tr("LLM error"), tr("The operation stopped and needs your attention."));
  }

  appendStatusMessage(tr("LLM error: %1").arg(error));

  if (m_editSession) {
    m_editSession->abort();
  }

  resetEditState();
}

void ChatWidget::onPlanFailed(const QString &reason) {
  if (!m_awaitingEdit) {
    return;
  }

  appendStatusMessage(reason);

  if (m_notifications) {
    m_notifications->notify(
        tr("Edit planning failed"),
        tr("The edit operation stopped and needs your attention."));
  }

  if (m_editSession) {
    m_editSession->abort();
  }

  resetEditState();
}

void ChatWidget::onEditCandidatesReady(const QVector<EditMatch> &candidates) {
  appendStatusMessage(
      tr("Multiple matches found · %1 candidates.").arg(candidates.size()));

  if (m_notifications) {
    m_notifications->notify(tr("Choose an edit target"),
                            tr("Multiple matching locations were found."));
  }

  if (m_currentEditNumber > 0 && m_awaitingEdit) {
    m_editSessionWidget->setStatus(m_currentEditNumber, tr("Ambiguous target"));
  }
}

void ChatWidget::onEditApplied(bool fuzzy, int editDistance) {
  if (m_currentEditNumber <= 0 || !m_awaitingEdit) {
    return;
  }

  const QString result =
      fuzzy ? tr("Applied using fuzzy matching · edit distance %1.")
                  .arg(editDistance)
            : tr("Applied.");

  m_editSessionWidget->finishEdit(m_currentEditNumber, result);

  appendStatusMessage(result);
}

void ChatWidget::onEditFailed(const QString &reason) {
  if (m_currentEditNumber > 0) {
    m_editSessionWidget->failEdit(m_currentEditNumber, reason);
  }

  if (m_notifications) {
    m_notifications->notify(tr("Edit failed"),
                            tr("An edit could not be completed."));
  }

  appendStatusMessage(
      tr("Edit %1 failed · %2").arg(m_currentEditNumber).arg(reason));

  resetEditState();
}

void ChatWidget::onEditAborted() {
  if (m_currentEditNumber > 0) {
    m_editSessionWidget->abortEdit(m_currentEditNumber, tr("Edit aborted."));
  }

  appendStatusMessage(tr("Edit aborted."));

  resetEditState();
}

void ChatWidget::onConflictsDetected() {
  m_planReadyToStream = false;

  appendStatusMessage(
      tr("Edit conflicts detected · resolve them before proceeding."));

  if (m_notifications) {
    m_notifications->notify(
        tr("Edit conflicts"),
        tr("Your attention is required to resolve edit conflicts."));
  }
}

void ChatWidget::onPlanReady(const QVector<EditCommand> &plannedCommands) {
  m_plannedEdits =
      QList<EditCommand>(plannedCommands.begin(), plannedCommands.end());

  m_editSessionWidget->clearHistory();

  for (int index = 0; index < m_plannedEdits.size(); ++index) {
    const EditCommand &command = m_plannedEdits.at(index);

    const int editNumber = index + 1;

    m_editSessionWidget->startEdit(editNumber, command.instruction);

    const QString commandJson =
        QString::fromUtf8(QJsonDocument(editCommandToJson(command))
                              .toJson(QJsonDocument::Indented));

    m_editSessionWidget->setCommand(editNumber, commandJson);

    m_editSessionWidget->setStatus(editNumber, tr("Queued"));
  }

  if (m_plannedEdits.isEmpty()) {
    appendStatusMessage(tr("No edits required."));

    resetEditState();
    return;
  }

  m_planReadyToStream = true;
  m_nextPlannedEditIndex = 0;
  m_currentEditNumber = 0;

  appendStatusMessage(tr("Plan ready · starting edits."));

  beginStreamingResolvedPlan();
}

void ChatWidget::onPendingEditAccepted(int editNumber) {
  if (!m_editSession) {
    return;
  }

  if (!m_editSession->setPendingEditAccepted(editNumber, true)) {
    return;
  }

  m_editSessionWidget->setPendingEditDecision(editNumber, true);
}

void ChatWidget::onPendingEditRejected(int editNumber) {
  if (!m_editSession) {
    return;
  }

  if (!m_editSession->setPendingEditAccepted(editNumber, false)) {
    return;
  }

  m_editSessionWidget->setPendingEditDecision(editNumber, false);
}

void ChatWidget::onAcceptAllPendingEdits() {
  if (!m_editSession) {
    return;
  }

  m_editSession->acceptAllPendingEdits();

  for (const PendingEdit &edit : m_editSession->pendingEdits()) {
    m_editSessionWidget->setPendingEditDecision(edit.id, true);
  }

  appendStatusMessage(tr("All edits marked for acceptance."));
}

void ChatWidget::onRejectAllPendingEdits() {
  if (!m_editSession) {
    return;
  }

  m_editSession->rejectAllPendingEdits();

  for (const PendingEdit &edit : m_editSession->pendingEdits()) {
    m_editSessionWidget->setPendingEditDecision(edit.id, false);
  }

  appendStatusMessage(tr("All edits marked for rejection."));
}

void ChatWidget::onApplyAcceptedPendingEdits() {
  if (!m_editSession) {
    return;
  }

  if (!m_editSession->applyAcceptedPendingEdits()) {
    appendStatusMessage(tr("Unable to apply the selected edits."));
    return;
  }

  if (m_activeEditor) {
    m_activeEditor->clearPendingEdits();
  }

  m_editSessionWidget->clearHistory();

  appendStatusMessage(tr("Selected edits applied."));

  resetEditState();
}

void ChatWidget::onReviewReady() {
  appendStatusMessage(tr("All planned edits are ready for review."));

  if (m_notifications) {
    m_notifications->notify(tr("Edits ready"),
                            tr("The generated edits are ready for review."));
  }

  if (!m_activeEditor || !m_activeEditor->autoAcceptEdits()) {
    return;
  }

  m_editSession->acceptAllPendingEdits();

  for (const PendingEdit &edit : m_editSession->pendingEdits()) {
    m_editSessionWidget->setPendingEditDecision(edit.id, true);
  }

  appendStatusMessage(tr("Auto-accept marked all edits for acceptance. "
                         "Review and apply them when ready."));
}

void ChatWidget::onConflictResolved(int groupId, int keepEditNumber) {
  Q_UNUSED(groupId);
  Q_UNUSED(keepEditNumber);

  appendStatusMessage(
      tr("Conflict selection received, but the current "
         "EditSession API does not expose a conflict-resolution method."));
}

void ChatWidget::onConflictGroupDiscarded(int groupId) {
  Q_UNUSED(groupId);

  appendStatusMessage(
      tr("Conflict discard received, but the current "
         "EditSession API does not expose a conflict-group discard method."));
}

void ChatWidget::onConflictBatchAborted() {
  if (m_editPlanner) {
    m_editPlanner->abort();
  }

  if (m_editSession) {
    m_editSession->abort();
  }

  m_editSessionWidget->clearHistory();

  if (m_notifications) {
    m_notifications->notify(tr("Edit operation stopped"),
                            tr("The batch was aborted."));
  }

  appendStatusMessage(tr("Batch aborted."));

  resetEditState();
}

void ChatWidget::appendUserMessage(const QString &text) {
  if (!m_transcript) {
    return;
  }

  QTextCursor cursor = m_transcript->textCursor();

  cursor.movePosition(QTextCursor::End);

  if (!cursor.atBlockStart() && !m_transcript->document()->isEmpty()) {
    appendMessageSeparator(cursor);
  }

  QTextBlockFormat blockFormat = messageBlockFormat();

  cursor.insertBlock(blockFormat);

  QTextCharFormat sender = senderFormat();

  cursor.insertText(tr("You"), sender);

  cursor.insertText(QStringLiteral("\n"));

  cursor.insertText(text);

  m_transcript->setTextCursor(cursor);
  m_transcript->ensureCursorVisible();

  m_assistantMessageOpen = false;
}

void ChatWidget::appendAssistantChunk(const QString &text) {
  if (!m_transcript) {
    return;
  }

  QTextCursor cursor = m_transcript->textCursor();

  cursor.movePosition(QTextCursor::End);

  if (!m_assistantMessageOpen) {
    if (!cursor.atBlockStart() && !m_transcript->document()->isEmpty()) {
      appendMessageSeparator(cursor);
    }

    QTextBlockFormat blockFormat = messageBlockFormat();

    cursor.insertBlock(blockFormat);

    QTextCharFormat sender = senderFormat();

    cursor.insertText(tr("Assistant"), sender);

    cursor.insertText(QStringLiteral("\n"));

    m_assistantMessageOpen = true;
  }

  cursor.movePosition(QTextCursor::End);
  cursor.insertText(text);

  m_transcript->setTextCursor(cursor);
  m_transcript->ensureCursorVisible();
}

void ChatWidget::appendStatusMessage(const QString &text) {
  if (!m_transcript) {
    return;
  }

  QTextCursor cursor = m_transcript->textCursor();

  cursor.movePosition(QTextCursor::End);

  if (!cursor.atBlockStart() && !m_transcript->document()->isEmpty()) {
    appendMessageSeparator(cursor);
  }

  QTextBlockFormat blockFormat = statusBlockFormat();

  cursor.insertBlock(blockFormat);

  QTextCharFormat format = statusFormat();

  cursor.insertText(text, format);

  m_transcript->setTextCursor(cursor);
  m_transcript->ensureCursorVisible();

  m_assistantMessageOpen = false;
}

void ChatWidget::renderLastAssistantMessage() {
  if (!m_transcript) {
    return;
  }

  m_transcript->ensureCursorVisible();
}

void ChatWidget::resetEditState() {
  m_plannedEdits.clear();

  m_nextPlannedEditIndex = 0;
  m_currentEditNumber = 0;

  m_currentCommandJson.clear();
  m_currentCommandDescription.clear();
  m_currentEditInstruction.clear();

  m_planReadyToStream = false;
  m_editPhase = EditPhase::None;
  m_awaitingEdit = false;
  m_editGenerationStopped = false;
  m_editAbortRequested = false;
}

QString ChatWidget::describeCommand(const EditCommand &command) const {
  QString operation;

  switch (command.operation) {
  case EditCommand::Operation::Insert:
    operation = QStringLiteral("Insert");
    break;

  case EditCommand::Operation::Replace:
    operation = QStringLiteral("Replace");
    break;

  case EditCommand::Operation::Delete:
    operation = QStringLiteral("Delete");
    break;
  }

  QString position;

  switch (command.position) {
  case EditCommand::Position::Before:
    position = QStringLiteral("before");
    break;

  case EditCommand::Position::After:
    position = QStringLiteral("after");
    break;
  }

  QString description = QStringLiteral("%1 %2 scope %3")
                            .arg(operation, position, command.scopeId);

  if (!command.findString.isEmpty()) {
    description += QStringLiteral(" matching \"%1\"").arg(command.findString);
  }

  if (command.replaceAll) {
    description += QStringLiteral(" (all matches)");
  }

  return description;
}