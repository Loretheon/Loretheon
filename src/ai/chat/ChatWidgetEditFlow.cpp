#include "../../../include/ai/chat/ChatWidgetEditFlow.h"

#include "../../../include/ai/chat/ChatWidget.h"
#include "NotificationManager.h"

#include "../../../include/ai/edit/EditPlanner.h"
#include "../../../include/ai/edit/EditSession.h"

#include "../../../include/text/TextEdit.h"

#include "inference/InferenceService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "../../../include/ai/chat/ChatWidgetSerialization.h"
#include "../../../include/ai/edit/EditSessionWidget.h"

#include <QCheckBox>

ChatWidgetEditFlow::ChatWidgetEditFlow(ChatWidget *widget)
    : QObject(widget), m_widget(widget) {}

void ChatWidgetEditFlow::sendPrompt(const QString &prompt) {
  if (!m_widget->m_inferenceService) {
    m_widget->appendStatusMessage(
        QObject::tr("Inference service is unavailable."));

    return;
  }

  if (m_widget->m_editPlanner) {
    m_widget->m_editPlanner->abort();
  }

  if (m_widget->m_editSession) {
    m_widget->m_editSession->abort();
  }

  m_widget->m_inferenceService->abortChatRequest();

  resetState();

  m_widget->m_currentLlmResponse.clear();

  m_widget->m_currentEditRequest = prompt;

  m_widget->appendUserMessage(prompt);

  const bool editMode =
      m_widget->m_editModeCheckbox && m_widget->m_editModeCheckbox->isChecked();

  if (editMode) {
    m_widget->m_awaitingEdit = true;

    if (!m_widget->m_activeEditor) {
      m_widget->appendStatusMessage(QObject::tr("No active document."));

      resetState();

      return;
    }

    if (!m_widget->m_editPlanner) {
      m_widget->appendStatusMessage(QObject::tr("Edit planner unavailable."));

      resetState();

      return;
    }

    m_widget->m_editSessionWidget->clearHistory();

    requestNextEditCommand();

    return;
  }

  QJsonArray messages;

  messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                              {QStringLiteral("content"), prompt}});

  m_widget->m_inferenceService->sendChatRequest(messages, QString(), 0.7,
                                                120000);
}

void ChatWidgetEditFlow::requestNextEditCommand() {
  if (!m_widget->m_editPlanner || !m_widget->m_activeEditor ||
      !m_widget->m_awaitingEdit) {
    return;
  }

  m_widget->appendStatusMessage(QObject::tr("Planning edits…"));

  m_widget->m_editPlanner->start(m_widget->m_activeEditor,
                                 m_widget->m_currentEditRequest);
}

void ChatWidgetEditFlow::beginStreamingResolvedPlan() {
  if (!m_widget->m_awaitingEdit || !m_widget->m_planReadyToStream) {
    return;
  }

  executeNextPlannedEdit();
}

void ChatWidgetEditFlow::executeNextPlannedEdit() {
  if (!m_widget->m_awaitingEdit || !m_widget->m_planReadyToStream) {
    return;
  }

  if (m_widget->m_nextPlannedEditIndex >=
      static_cast<size_t>(m_widget->m_plannedEdits.size())) {
    m_widget->m_editPhase = ChatWidget::EditPhase::None;

    m_widget->m_awaitingEdit = false;

    m_widget->appendStatusMessage(
        QObject::tr("All planned edits are ready for review."));

    if (m_widget->m_notifications) {
      m_widget->m_notifications->notify(
          QObject::tr("Edits ready"),
          QObject::tr("The generated edits are ready for review."));
    }

    return;
  }

  m_widget->m_currentEditNumber =
      static_cast<int>(m_widget->m_nextPlannedEditIndex + 1);

  const EditCommand command =
      m_widget->m_plannedEdits.at(m_widget->m_currentEditNumber - 1);

  m_widget->m_currentEditInstruction = command.instruction;

  m_widget->m_currentCommandDescription = describeCommand(command);

  m_widget->m_editSessionWidget->setStatus(m_widget->m_currentEditNumber,
                                           QObject::tr("Resolving"));

  if (!m_widget->m_editSession->prepareStreaming(
          command, m_widget->m_currentEditNumber)) {
    m_widget->m_editSessionWidget->failEdit(
        m_widget->m_currentEditNumber,
        QObject::tr("Unable to resolve target."));

    resetState();

    return;
  }

  if (command.operation == EditCommand::Operation::Delete) {
    return;
  }

  m_widget->m_editSessionWidget->setStatus(m_widget->m_currentEditNumber,
                                           QObject::tr("Writing"));

  requestEditContent();
}

void ChatWidgetEditFlow::requestEditContent() {
  if (!m_widget->m_inferenceService || !m_widget->m_activeEditor ||
      !m_widget->m_awaitingEdit) {
    return;
  }

  const QString instruction = m_widget->m_currentEditInstruction.trimmed();

  const QString description = m_widget->m_currentCommandDescription.trimmed();

  QJsonArray messages;

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("system")},
      {QStringLiteral("content"),
       QStringLiteral("You are an automated document text generator. "
                      "Output only Markdown text.")}});

  messages.append(
      QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                  {QStringLiteral("content"),
                   QStringLiteral("Generate replacement content.\n\n"
                                  "Instruction:\n%1\n\n"
                                  "Target:\n%2")
                       .arg(instruction, description)}});

  m_widget->m_editPhase = ChatWidget::EditPhase::Content;

  m_widget->m_currentLlmResponse.clear();

  m_widget->m_inferenceService->sendChatRequest(messages, QString(), 0.7,
                                                120000);
}

void ChatWidgetEditFlow::onPlanReady(const QVector<EditCommand> &commands) {
  m_widget->m_plannedEdits =
      QList<EditCommand>(commands.begin(), commands.end());

  m_widget->m_editSessionWidget->clearHistory();

  for (int i = 0; i < m_widget->m_plannedEdits.size(); ++i) {
    const EditCommand &command = m_widget->m_plannedEdits.at(i);

    const int number = i + 1;

    m_widget->m_editSessionWidget->startEdit(number, command.instruction);

    QString json = QString::fromUtf8(
        QJsonDocument(ChatWidgetSerialization::editCommandToJson(command))
            .toJson(QJsonDocument::Indented));

    m_widget->m_editSessionWidget->setCommand(number, json);

    m_widget->m_editSessionWidget->setStatus(number, QObject::tr("Queued"));
  }

  if (m_widget->m_plannedEdits.isEmpty()) {
    m_widget->appendStatusMessage(QObject::tr("No edits required."));

    resetState();

    return;
  }

  m_widget->m_planReadyToStream = true;
  m_widget->m_nextPlannedEditIndex = 0;
  m_widget->m_currentEditNumber = 0;

  beginStreamingResolvedPlan();
}

void ChatWidgetEditFlow::onPlanFailed(const QString &reason) {
  if (!m_widget->m_awaitingEdit) {
    return;
  }

  m_widget->appendStatusMessage(reason);

  if (m_widget->m_editSession) {
    m_widget->m_editSession->abort();
  }

  resetState();
}

void ChatWidgetEditFlow::onEditCandidatesReady(
    const QVector<EditMatch> &candidates) {
  m_widget->appendStatusMessage(
      QObject::tr("Multiple matches found · %1 candidates.")
          .arg(candidates.size()));

  if (m_widget->m_currentEditNumber > 0) {
    m_widget->m_editSessionWidget->setStatus(m_widget->m_currentEditNumber,
                                             QObject::tr("Ambiguous target"));
  }
}

void ChatWidgetEditFlow::onEditApplied(bool fuzzy, int distance) {
  if (m_widget->m_currentEditNumber <= 0) {
    return;
  }

  QString result =
      fuzzy ? QObject::tr("Applied using fuzzy matching · edit distance %1.")
                  .arg(distance)
            : QObject::tr("Applied.");

  m_widget->m_editSessionWidget->finishEdit(m_widget->m_currentEditNumber,
                                            result);

  m_widget->appendStatusMessage(result);
}

void ChatWidgetEditFlow::onEditFailed(const QString &reason) {
  if (m_widget->m_currentEditNumber > 0) {
    m_widget->m_editSessionWidget->failEdit(m_widget->m_currentEditNumber,
                                            reason);
  }

  m_widget->appendStatusMessage(reason);

  resetState();
}

void ChatWidgetEditFlow::onEditAborted() {
  if (m_widget->m_currentEditNumber > 0) {
    m_widget->m_editSessionWidget->abortEdit(m_widget->m_currentEditNumber,
                                             QObject::tr("Edit aborted."));
  }

  resetState();
}

void ChatWidgetEditFlow::onReviewReady() {
  m_widget->appendStatusMessage(
      QObject::tr("All planned edits are ready for review."));
}

void ChatWidgetEditFlow::resetState() {
  m_widget->m_plannedEdits.clear();
  m_widget->m_nextPlannedEditIndex = 0;
  m_widget->m_currentEditNumber = 0;

  m_widget->m_currentEditInstruction.clear();
  m_widget->m_currentCommandDescription.clear();

  m_widget->m_planReadyToStream = false;
  m_widget->m_awaitingEdit = false;
  m_widget->m_editGenerationStopped = false;
  m_widget->m_editAbortRequested = false;
}

QString ChatWidgetEditFlow::describeCommand(const EditCommand &command) const {
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

  return QStringLiteral("%1 %2 scope %3")
      .arg(operation, position, command.scopeId);
}