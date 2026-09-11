#include "ChatWidget.h"

#include "EditGrammar.h"
#include "EditSessionWidget.h"
#include "edit/EditPlanner.h"
#include "edit/EditSession.h"

#include "../text/TextEdit.h"

#include "inference/InferenceService.h"

#include <QCheckBox>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QSplitter>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString editCommandOperationString(
    const EditCommand &command) {
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

QString editCommandPositionString(
    const EditCommand &command) {
  switch (command.position) {
  case EditCommand::Position::Before:
    return QStringLiteral("before");

  case EditCommand::Position::After:
    return QStringLiteral("after");
  }

  return {};
}

QJsonObject editCommandToJson(
    const EditCommand &command) {
  QJsonObject object;

  object.insert(
      QStringLiteral("operation"),
      editCommandOperationString(command));

  object.insert(
      QStringLiteral("scope"),
      command.scopeId);

  object.insert(
      QStringLiteral("position"),
      editCommandPositionString(command));

  object.insert(
      QStringLiteral("find"),
      command.findString);

  object.insert(
      QStringLiteral("all"),
      command.replaceAll);

  object.insert(
      QStringLiteral("instruction"),
      command.instruction);

  return object;
}

} // namespace

ChatWidget::ChatWidget(
    InferenceService *inferenceService,
    EditSession *editSession,
    QWidget *parent)
    : QWidget(parent),
      m_inferenceService(inferenceService),
      m_editSession(editSession) {
  m_transcript = new QTextEdit(this);

  m_transcript->setReadOnly(true);
  m_transcript->setAcceptRichText(true);
  m_transcript->setLineWrapMode(
      QTextEdit::WidgetWidth);
  m_transcript->setFrameShape(
      QFrame::NoFrame);

  m_transcript->setStyleSheet(
      QStringLiteral(
          "QTextEdit {"
          "    background: palette(base);"
          "    border: none;"
          "    padding: 14px;"
          "}"));

  m_input = new QLineEdit(this);

  m_input->setPlaceholderText(
      tr("Ask the assistant…"));

  m_input->setClearButtonEnabled(true);
  m_input->setMinimumHeight(38);

  m_input->setStyleSheet(
      QStringLiteral(
          "QLineEdit {"
          "    border: 1px solid palette(mid);"
          "    border-radius: 7px;"
          "    padding: 7px 10px;"
          "}"
          "QLineEdit:focus {"
          "    border: 1px solid #4a90e2;"
          "}"));

  m_sendButton =
      new QPushButton(
          tr("Send"),
          this);

  m_sendButton->setMinimumHeight(38);
  m_sendButton->setMinimumWidth(76);

  m_sendButton->setStyleSheet(
      QStringLiteral(
          "QPushButton {"
          "    border: 1px solid #357abd;"
          "    border-radius: 7px;"
          "    background: #4285d4;"
          "    color: white;"
          "    padding: 0 14px;"
          "    font-weight: 600;"
          "}"
          "QPushButton:hover {"
          "    background: #357abd;"
          "}"
          "QPushButton:pressed {"
          "    background: #2d6ca2;"
          "}"
          "QPushButton:disabled {"
          "    background: palette(mid);"
          "    border-color: palette(mid);"
          "}"));

  m_editModeCheckbox =
      new QCheckBox(
          tr("Edit document"),
          this);

  m_editModeCheckbox->setToolTip(
      tr("Plan and preview document edits "
         "before applying them."));

  m_editModeCheckbox->setStyleSheet(
      QStringLiteral(
          "QCheckBox {"
          "    spacing: 6px;"
          "    padding: 4px;"
          "}"));

  m_editSessionWidget =
      new EditSessionWidget(this);

  if (m_inferenceService) {
    m_editPlanner =
        new EditPlanner(
            m_inferenceService,
            this);

    connect(
        m_editPlanner,
        &EditPlanner::planReady,
        this,
        &ChatWidget::onPlanReady);

    connect(
        m_editPlanner,
        &EditPlanner::failed,
        this,
        &ChatWidget::onPlanFailed);
  }

  auto *contentSplitter =
      new QSplitter(
          Qt::Horizontal,
          this);

  contentSplitter->setChildrenCollapsible(false);
  contentSplitter->setHandleWidth(6);

  contentSplitter->addWidget(
      m_transcript);

  contentSplitter->addWidget(
      m_editSessionWidget);

  contentSplitter->setSizes({
      680,
      420});

  contentSplitter->setStretchFactor(
      0,
      1);

  contentSplitter->setStretchFactor(
      1,
      0);

  contentSplitter->setStyleSheet(
      QStringLiteral(
          "QSplitter::handle {"
          "    background: palette(midlight);"
          "}"));

  auto *controlsFrame =
      new QWidget(this);

  controlsFrame->setObjectName(
      QStringLiteral(
          "chatControls"));

  controlsFrame->setStyleSheet(
      QStringLiteral(
          "#chatControls {"
          "    border-top: 1px solid palette(mid);"
          "    background: palette(window);"
          "}"));

  auto *controlsLayout =
      new QHBoxLayout(
          controlsFrame);

  controlsLayout->setContentsMargins(
      10,
      8,
      10,
      10);

  controlsLayout->setSpacing(8);

  controlsLayout->addWidget(
      m_editModeCheckbox);

  controlsLayout->addWidget(
      m_sendButton);

  controlsLayout->addWidget(
      m_input,
      1);

  m_layout =
      new QVBoxLayout(this);

  m_layout->setContentsMargins(
      0,
      0,
      0,
      0);

  m_layout->setSpacing(0);

  m_layout->addWidget(
      contentSplitter,
      1);

  m_layout->addWidget(
      controlsFrame);

  setLayout(m_layout);

  connect(
      m_sendButton,
      &QPushButton::clicked,
      this,
      &ChatWidget::onSendClicked);

  connect(
      m_input,
      &QLineEdit::returnPressed,
      this,
      &ChatWidget::onSendClicked);

  if (m_inferenceService) {
    connect(
        m_inferenceService,
        &InferenceService::llmDelta,
        this,
        &ChatWidget::onLlmDelta);

    connect(
        m_inferenceService,
        &InferenceService::llmFinished,
        this,
        &ChatWidget::onLlmFinished);

    connect(
        m_inferenceService,
        &InferenceService::llmError,
        this,
        &ChatWidget::onLlmError);
  }

  if (m_editSession) {
    connect(
        m_editSession,
        &EditSession::candidatesReady,
        this,
        &ChatWidget::onEditCandidatesReady);

    connect(
        m_editSession,
        &EditSession::applied,
        this,
        &ChatWidget::onEditApplied);

    connect(
        m_editSession,
        &EditSession::failed,
        this,
        &ChatWidget::onEditFailed);

    connect(
        m_editSession,
        &EditSession::aborted,
        this,
        &ChatWidget::onEditAborted);

    connect(
        m_editSession,
        &EditSession::conflictsDetected,
        this,
        &ChatWidget::onConflictsDetected);

    connect(
        m_editSession,
        &EditSession::planReady,
        this,
        &ChatWidget::onPlanReady);

    connect(
        m_editSession,
        &EditSession::reviewReady,
        this,
        &ChatWidget::onReviewReady);

    connect(
        m_editSession,
        &EditSession::pendingEditStarted,
        this,
        [this](const PendingEdit &edit) {
          if (m_activeEditor) {
            m_activeEditor->showPendingEdit(
                edit);
          }
        });

    connect(
        m_editSession,
        &EditSession::pendingEditUpdated,
        this,
        [this](const PendingEdit &edit) {
          if (m_activeEditor) {
            m_activeEditor->updatePendingEdit(
                edit);
          }
        });

    connect(
        m_editSession,
        &EditSession::pendingEditFinished,
        this,
        [this](const PendingEdit &edit) {

          if (edit.id <= 0) {
            return;
          }

          m_editSessionWidget
              ->setPendingEditReady(
                  edit.id);

          if (m_activeEditor &&
              m_activeEditor->autoAcceptEdits()) {
            onPendingEditAccepted(
                edit.id);
          }

          if (!m_awaitingEdit ||
              m_editPhase ==
                  EditPhase::None) {
            return;
          }

          if (edit.id !=
              m_currentEditNumber) {
            return;
          }

          ++m_nextPlannedEditIndex;

          m_currentEditNumber = 0;

          m_currentEditInstruction.clear();
          m_currentCommandDescription.clear();

          /*
           * Do not start the next edit from inside
           * EditSession::finishStreaming().
           *
           * finishStreaming() still has bookkeeping to
           * complete after emitting pendingEditFinished().
           * Starting the next request synchronously here
           * re-enters EditSession and can leave its state
           * belonging to the previous edit.
           */
          if (m_nextPlannedEditIndex >=
              static_cast<size_t>(
                  m_plannedEdits.size())) {

            m_editPhase =
                EditPhase::None;

            m_editGenerationStopped =
                false;

            m_editAbortRequested =
                false;

            return;
          }

          m_editGenerationStopped =
              false;

          m_editAbortRequested =
              false;

          QMetaObject::invokeMethod(
              this,
              [this]() {
                if (!m_awaitingEdit ||
                    m_editPhase ==
                        EditPhase::None) {
                  return;
                }

                executeNextPlannedEdit();
              },
              Qt::QueuedConnection);
        });

    connect(
        m_editSession,
        &EditSession::pendingEditsChanged,
        this,
        [this]() {
          if (m_activeEditor) {
            m_activeEditor
                ->refreshPendingEdits();
          }
        });
  }

  connect(
      m_editSessionWidget,
      &EditSessionWidget::pendingEditAccepted,
      this,
      &ChatWidget::onPendingEditAccepted);

  connect(
      m_editSessionWidget,
      &EditSessionWidget::pendingEditRejected,
      this,
      &ChatWidget::onPendingEditRejected);

  connect(
      m_editSessionWidget,
      &EditSessionWidget::
          acceptAllPendingEditsRequested,
      this,
      &ChatWidget::
          onAcceptAllPendingEdits);

  connect(
      m_editSessionWidget,
      &EditSessionWidget::
          rejectAllPendingEditsRequested,
      this,
      &ChatWidget::
          onRejectAllPendingEdits);

  connect(
      m_editSessionWidget,
      &EditSessionWidget::
          applyAcceptedPendingEditsRequested,
      this,
      &ChatWidget::
          onApplyAcceptedPendingEdits);

  connect(
      m_editSessionWidget,
      &EditSessionWidget::skipRequested,
      this,
      [this]() {

        if (m_editPlanner) {
          m_editPlanner->abort();
        }

        if (m_inferenceService) {
          m_editGenerationStopped =
              true;

          m_editAbortRequested =
              true;

          m_inferenceService
              ->abortChatRequest();
        }

        if (m_editSession) {
          m_editSession->abort();
        }

        resetEditState();
      });

  connect(
      m_editSessionWidget,
      &EditSessionWidget::conflictResolved,
      this,
      &ChatWidget::onConflictResolved);

  connect(
      m_editSessionWidget,
      &EditSessionWidget::
          conflictGroupDiscarded,
      this,
      &ChatWidget::
          onConflictGroupDiscarded);

  connect(
      m_editSessionWidget,
      &EditSessionWidget::
          conflictBatchAborted,
      this,
      &ChatWidget::
          onConflictBatchAborted);
}

void ChatWidget::setActiveEditor(
    TextEdit *editor) {
  m_activeEditor =
      editor;

  if (m_editSession) {
    m_editSession->setEditor(
        editor);
  }

  if (m_activeEditor &&
      m_editSession) {
    m_activeEditor
        ->refreshPendingEdits();
  }
}

void ChatWidget::submitTranscribedText(
    const QString &text) {
  if (text.trimmed().isEmpty()) {
    return;
  }

  m_input->setText(text);

  sendPrompt(text);
}

void ChatWidget::onSendClicked() {
  const QString prompt =
      m_input->text().trimmed();

  if (prompt.isEmpty()) {
    return;
  }

  m_input->clear();

  sendPrompt(prompt);
}

void ChatWidget::sendPrompt(
    const QString &prompt) {
  if (!m_inferenceService) {
    appendStatusMessage(
        tr("Inference service is unavailable."));
    return;
  }

  if (m_editPlanner) {
    m_editPlanner->abort();
  }

  if (m_editSession) {
    m_editSession->abort();
  }

  m_inferenceService
      ->abortChatRequest();

  resetEditState();

  m_currentLlmResponse.clear();

  m_currentEditRequest =
      prompt;

  appendUserMessage(prompt);

  m_payloadLogger.log(
      QStringLiteral("USER_PROMPT"),
      QStringLiteral(
          "Prompt: \"%1\"")
          .arg(prompt));

  const bool editMode =
      m_editModeCheckbox &&
      m_editModeCheckbox->isChecked();

  if (editMode) {
    m_awaitingEdit = true;

    if (!m_activeEditor) {
      appendStatusMessage(
          tr("No active document."));
      resetEditState();
      return;
    }

    if (!m_editPlanner) {
      appendStatusMessage(
          tr("Edit planner is unavailable."));
      resetEditState();
      return;
    }

    m_editSessionWidget
        ->clearHistory();

    requestNextEditCommand();

    return;
  }

  QJsonArray messages;

  messages.append(
      QJsonObject{
          {QStringLiteral("role"),
           QStringLiteral("user")},
          {QStringLiteral("content"),
           prompt}});

  m_payloadLogger.log(
      QStringLiteral("CHAT_REQUEST"),
      QStringLiteral(
          "Prompt: \"%1\"")
          .arg(prompt));

  m_inferenceService
      ->sendChatRequest(
          messages,
          QString(),
          0.7,
          120000);
}

void ChatWidget::requestNextEditCommand() {
  if (!m_editPlanner ||
      !m_activeEditor ||
      !m_awaitingEdit) {
    return;
  }

  appendStatusMessage(
      tr("Planning edits..."));

  m_editPlanner->start(
      m_activeEditor,
      m_currentEditRequest);
}

void ChatWidget::beginStreamingResolvedPlan() {
  if (!m_awaitingEdit ||
      !m_planReadyToStream) {
    return;
  }

  executeNextPlannedEdit();
}

void ChatWidget::executeNextPlannedEdit() {
  if (!m_awaitingEdit ||
      !m_planReadyToStream) {
    return;
  }

  if (m_nextPlannedEditIndex >=
      static_cast<size_t>(
          m_plannedEdits.size())) {

    m_editPhase =
        EditPhase::None;

    m_awaitingEdit =
        false;

    appendStatusMessage(
        tr("All planned edits are ready for review."));

    return;
  }

  m_currentEditNumber =
      static_cast<int>(
          m_nextPlannedEditIndex + 1);

  const EditCommand command =
      m_plannedEdits.at(
          static_cast<int>(
              m_nextPlannedEditIndex));

  m_currentEditInstruction =
      command.instruction;

  m_currentCommandDescription =
      describeCommand(command);

  m_editSessionWidget
      ->setStatus(
          m_currentEditNumber,
          tr("Resolving"));

  appendStatusMessage(
      tr("Edit %1: resolving target...")
          .arg(m_currentEditNumber));

  if (!m_editSession->prepareStreaming(
          command,
          m_currentEditNumber)) {

    m_editSessionWidget
        ->setStatus(
            m_currentEditNumber,
            tr("Failed"));

    m_editSessionWidget
        ->failEdit(
            m_currentEditNumber,
            tr("Unable to resolve the requested target."));

    resetEditState();
    return;
  }

  if (command.operation ==
      EditCommand::Operation::Delete) {
    return;
  }

  m_editSessionWidget
      ->setStatus(
          m_currentEditNumber,
          tr("Writing"));

  requestEditContent();
}

void ChatWidget::requestEditContent() {
  if (!m_inferenceService ||
      !m_activeEditor ||
      !m_awaitingEdit) {
    return;
  }

  QString cleanInstruction =
      m_currentEditInstruction.trimmed();

  QString cleanDescription =
      m_currentCommandDescription.trimmed();

  if (cleanDescription.startsWith('[') &&
      cleanDescription.endsWith(']')) {
    cleanDescription.remove(0, 1);
    cleanDescription.chop(1);
    cleanDescription =
        cleanDescription.trimmed();
  }

  QJsonArray messages;

  const QString systemPrompt =
      QStringLiteral(
          "You are an automated document text generator. "
          "You output raw Markdown text directly. "
          "Never output JSON, punctuation wrappers, or closing brackets like ']'. ");

  const QString contentPrompt =
      QStringLiteral(
          "TASK: Generate the replacement Markdown content for a single "
          "document edit.\n\n"
          "INSTRUCTIONS:\n"
          "- Output ONLY the final text/paragraph to insert or replace.\n"
          "- Do not write JSON.\n"
          "- Do not write explanations.\n"
          "- Do not output leading closing brackets or syntax symbols.\n\n"
          "EDIT INSTRUCTION:\n%1\n\n"
          "TARGET DETAILS:\n%2\n\n"
          "BEGIN MARKDOWN CONTENT:")
          .arg(
              cleanInstruction,
              cleanDescription);

  messages.append(
      QJsonObject{
          {QStringLiteral("role"),
           QStringLiteral("system")},
          {QStringLiteral("content"),
           systemPrompt}});

  messages.append(
      QJsonObject{
          {QStringLiteral("role"),
           QStringLiteral("user")},
          {QStringLiteral("content"),
           contentPrompt}});

  m_editPhase =
      EditPhase::Content;

  m_currentLlmResponse.clear();

  const QString currentScopeId =
      (m_currentEditNumber > 0 &&
       m_currentEditNumber <=
           m_plannedEdits.size())
          ? m_plannedEdits
                .at(
                    m_currentEditNumber - 1)
                .scopeId
          : QStringLiteral("unknown");

  m_payloadLogger.log(
      QStringLiteral(
          "MARKDOWN_GEN_REQUEST"),
      QStringLiteral(
          "Edit #%1 | Scope: %2 | Instruction: \"%3\"")
          .arg(
              QString::number(
                  m_currentEditNumber),
              currentScopeId,
              cleanInstruction));

  m_inferenceService
      ->sendChatRequest(
          messages,
          QString(),
          0.7,
          120000);
}

void ChatWidget::onLlmDelta(
    const QString &text) {
  if (text.isEmpty()) {
    return;
  }

  m_currentLlmResponse += text;

  if (m_awaitingEdit) {
    if (m_editPhase !=
            EditPhase::Content ||
        m_editGenerationStopped) {
      return;
    }

    if (m_editSession) {
      m_editSession
          ->appendStreaming(text);
    }

    return;
  }

  appendAssistantChunk(text);
}

void ChatWidget::onLlmFinished() {
  if (m_awaitingEdit &&
      m_editPhase ==
          EditPhase::Content) {

    m_payloadLogger.log(
        QStringLiteral(
            "MARKDOWN_GEN_RESPONSE"),
        QStringLiteral(
            "Edit #%1\n%2")
            .arg(
                QString::number(
                    m_currentEditNumber),
                m_currentLlmResponse));

  } else if (!m_currentLlmResponse.isEmpty()) {

    m_payloadLogger.log(
        QStringLiteral(
            "CHAT_RESPONSE"),
        m_currentLlmResponse);
  }

  m_currentLlmResponse.clear();

  if (!m_awaitingEdit) {
    if (m_assistantMessageOpen) {
      renderLastAssistantMessage();
    }

    m_assistantMessageOpen =
        false;

    return;
  }

  if (m_editAbortRequested) {
    return;
  }

  if (m_editPhase ==
          EditPhase::Content &&
      m_editSession) {

    m_editSession
        ->finishStreaming();
  }
}

void ChatWidget::onLlmError(
    const QString &error) {
  m_payloadLogger.log(
      QStringLiteral(
          "MARKDOWN_GEN_ERROR"),
      QStringLiteral(
          "Edit #%1 Error: %2")
          .arg(
              QString::number(
                  m_currentEditNumber),
              error));

  m_currentLlmResponse.clear();

  if (m_awaitingEdit &&
      m_editPhase !=
          EditPhase::Content) {
    return;
  }

  if (m_editAbortRequested) {
    return;
  }

  appendStatusMessage(
      tr("LLM error: %1")
          .arg(error));

  if (m_editSession) {
    m_editSession
        ->abort();
  }

  resetEditState();
}

void ChatWidget::onPlanFailed(
    const QString &reason) {
  if (!m_awaitingEdit) {
    return;
  }

  appendStatusMessage(reason);

  if (m_editSession) {
    m_editSession->abort();
  }

  resetEditState();
}

void ChatWidget::onEditCandidatesReady(
    const QVector<EditMatch> &candidates) {
  appendStatusMessage(
      tr("Multiple matches found (%1 candidates).")
          .arg(candidates.size()));

  if (m_currentEditNumber > 0 &&
      m_awaitingEdit) {

    m_editSessionWidget->setStatus(
        m_currentEditNumber,
        tr("Ambiguous target"));
  }
}

void ChatWidget::onEditApplied(
    bool fuzzy,
    int editDistance) {
  if (m_currentEditNumber <= 0 ||
      !m_awaitingEdit) {
    return;
  }

  const QString result =
      fuzzy
          ? tr("Applied using fuzzy matching "
               "(edit distance %1).")
                .arg(editDistance)
          : tr("Applied.");

  m_editSessionWidget
      ->finishEdit(
          m_currentEditNumber,
          result);

  appendStatusMessage(result);
}

void ChatWidget::onEditFailed(
    const QString &reason) {
  if (m_currentEditNumber > 0) {
    m_editSessionWidget
        ->failEdit(
            m_currentEditNumber,
            reason);
  }

  appendStatusMessage(
      tr("Edit %1 failed: %2")
          .arg(m_currentEditNumber)
          .arg(reason));

  resetEditState();
}

void ChatWidget::onEditAborted() {
  if (m_currentEditNumber > 0) {
    m_editSessionWidget
        ->abortEdit(
            m_currentEditNumber,
            tr("Edit aborted."));
  }

  appendStatusMessage(
      tr("Edit aborted."));

  resetEditState();
}

void ChatWidget::onConflictsDetected() {
  m_planReadyToStream =
      false;

  appendStatusMessage(
      tr("Edit conflicts detected. "
         "Please resolve conflicts before proceeding."));
}

void ChatWidget::onPlanReady(
    const QVector<EditCommand> &plannedCommands) {
  m_plannedEdits =
      QList<EditCommand>(
          plannedCommands.begin(),
          plannedCommands.end());

  m_editSessionWidget
      ->clearHistory();

  for (int i = 0;
       i < m_plannedEdits.size();
       ++i) {

    const EditCommand &command =
        m_plannedEdits.at(i);

    m_editSessionWidget
        ->startEdit(
            i + 1,
            command.instruction);

    const QString commandJson =
        QString::fromUtf8(
            QJsonDocument(
                editCommandToJson(command))
                .toJson(
                    QJsonDocument::Indented));

    m_editSessionWidget
        ->setCommand(
            i + 1,
            commandJson);

    m_editSessionWidget
        ->setStatus(
            i + 1,
            tr("Queued"));
  }

  if (m_plannedEdits.isEmpty()) {
    appendStatusMessage(
        tr("No edits required."));

    resetEditState();
    return;
  }

  m_planReadyToStream =
      true;

  m_nextPlannedEditIndex =
      0;

  m_currentEditNumber =
      0;

  appendStatusMessage(
      tr("Plan clear, starting edits."));

  beginStreamingResolvedPlan();
}

void ChatWidget::onPendingEditAccepted(
    int editNumber) {
  if (!m_editSession) {
    return;
  }

  if (!m_editSession
           ->setPendingEditAccepted(
               editNumber,
               true)) {
    return;
  }

  m_editSessionWidget
      ->setPendingEditDecision(
          editNumber,
          true);
}

void ChatWidget::onPendingEditRejected(
    int editNumber) {
  if (!m_editSession) {
    return;
  }

  if (!m_editSession
           ->setPendingEditAccepted(
               editNumber,
               false)) {
    return;
  }

  m_editSessionWidget
      ->setPendingEditDecision(
          editNumber,
          false);
}

void ChatWidget::onAcceptAllPendingEdits() {
  if (!m_editSession) {
    return;
  }

  m_editSession
      ->acceptAllPendingEdits();

  for (const PendingEdit &edit :
       m_editSession
           ->pendingEdits()) {

    m_editSessionWidget
        ->setPendingEditDecision(
            edit.id,
            true);
  }

  appendStatusMessage(
      tr("All edits marked for acceptance."));
}

void ChatWidget::onRejectAllPendingEdits() {
  if (!m_editSession) {
    return;
  }

  m_editSession
      ->rejectAllPendingEdits();

  for (const PendingEdit &edit :
       m_editSession
           ->pendingEdits()) {

    m_editSessionWidget
        ->setPendingEditDecision(
            edit.id,
            false);
  }

  appendStatusMessage(
      tr("All edits marked for rejection."));
}

void ChatWidget::onApplyAcceptedPendingEdits() {
  if (!m_editSession) {
    return;
  }

  if (!m_editSession
           ->applyAcceptedPendingEdits()) {

    appendStatusMessage(
        tr("Unable to apply the selected edits."));

    return;
  }

  if (m_activeEditor) {
    m_activeEditor
        ->clearPendingEdits();
  }

  m_editSessionWidget
      ->clearHistory();

  appendStatusMessage(
      tr("Selected edits applied."));

  resetEditState();
}

void ChatWidget::onReviewReady() {
  appendStatusMessage(
      tr("All planned edits are ready for review."));

  if (!m_activeEditor ||
      !m_activeEditor->autoAcceptEdits()) {
    return;
  }

  m_editSession
      ->acceptAllPendingEdits();

  for (const PendingEdit &edit :
       m_editSession
           ->pendingEdits()) {

    m_editSessionWidget
        ->setPendingEditDecision(
            edit.id,
            true);
  }

  appendStatusMessage(
      tr("Auto-accept marked all edits for acceptance. "
         "Review and apply them when ready."));
}

void ChatWidget::onConflictResolved(
    int groupId,
    int keepEditNumber) {
  Q_UNUSED(groupId);
  Q_UNUSED(keepEditNumber);

  appendStatusMessage(
      tr("Conflict selection received, "
         "but the current EditSession API does not "
         "expose a conflict-resolution method."));
}

void ChatWidget::onConflictGroupDiscarded(
    int groupId) {
  Q_UNUSED(groupId);

  appendStatusMessage(
      tr("Conflict discard received, "
         "but the current EditSession API does not "
         "expose a conflict-group discard method."));
}

void ChatWidget::onConflictBatchAborted() {
  if (m_editPlanner) {
    m_editPlanner->abort();
  }

  if (m_editSession) {
    m_editSession->abort();
  }

  m_editSessionWidget
      ->clearHistory();

  appendStatusMessage(
      tr("Batch aborted."));

  resetEditState();
}

void ChatWidget::appendUserMessage(
    const QString &text) {
  if (!m_transcript) {
    return;
  }

  QTextCursor cursor =
      m_transcript->textCursor();

  cursor.movePosition(
      QTextCursor::End);

  QTextCharFormat format;

  format.setFontWeight(
      QFont::Bold);

  cursor.insertText(
      QStringLiteral("You:"),
      format);

  cursor.insertText(
      QStringLiteral(" "));

  cursor.insertText(text);

  cursor.insertText(
      QStringLiteral("\n\n"));

  m_transcript
      ->setTextCursor(cursor);

  m_transcript
      ->ensureCursorVisible();
}

void ChatWidget::appendAssistantChunk(
    const QString &text) {
  if (!m_transcript) {
    return;
  }

  if (!m_assistantMessageOpen) {
    m_transcript
        ->append(
            QStringLiteral(
                "<b>Assistant:</b>"));

    m_assistantMessageOpen =
        true;
  }

  QTextCursor cursor =
      m_transcript->textCursor();

  cursor.movePosition(
      QTextCursor::End);

  cursor.insertText(text);

  m_transcript
      ->setTextCursor(cursor);

  m_transcript
      ->ensureCursorVisible();
}

void ChatWidget::appendStatusMessage(
    const QString &text) {
  if (!m_transcript) {
    return;
  }

  QTextCursor cursor =
      m_transcript->textCursor();

  cursor.movePosition(
      QTextCursor::End);

  QTextCharFormat format;

  format.setFontItalic(true);

  cursor.insertText(
      text,
      format);

  cursor.insertText(
      QStringLiteral("\n\n"));

  m_transcript
      ->setTextCursor(cursor);

  m_transcript
      ->ensureCursorVisible();
}

void ChatWidget::renderLastAssistantMessage() {
  if (!m_transcript) {
    return;
  }
}

void ChatWidget::resetEditState() {
  m_plannedEdits.clear();

  m_nextPlannedEditIndex =
      0;

  m_currentEditNumber =
      0;

  m_currentCommandJson.clear();
  m_currentCommandDescription.clear();
  m_currentEditInstruction.clear();

  m_planReadyToStream =
      false;

  m_editPhase =
      EditPhase::None;

  m_awaitingEdit =
      false;

  m_editGenerationStopped =
      false;

  m_editAbortRequested =
      false;
}

QString ChatWidget::describeCommand(
    const EditCommand &command) const {
  QString operation;

  switch (command.operation) {
  case EditCommand::Operation::Insert:
    operation =
        QStringLiteral("Insert");
    break;

  case EditCommand::Operation::Replace:
    operation =
        QStringLiteral("Replace");
    break;

  case EditCommand::Operation::Delete:
    operation =
        QStringLiteral("Delete");
    break;
  }

  QString position;

  switch (command.position) {
  case EditCommand::Position::Before:
    position =
        QStringLiteral("before");
    break;

  case EditCommand::Position::After:
    position =
        QStringLiteral("after");
    break;
  }

  QString description =
      QStringLiteral(
          "%1 %2 scope %3")
          .arg(
              operation,
              position,
              command.scopeId);

  if (!command.findString.isEmpty()) {
    description +=
        QStringLiteral(
            " matching \"%1\"")
            .arg(
                command.findString);
  }

  if (command.replaceAll) {
    description +=
        QStringLiteral(
            " (all matches)");
  }

  return description;
}