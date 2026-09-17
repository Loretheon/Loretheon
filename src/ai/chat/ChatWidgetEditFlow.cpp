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
#include <QRegularExpression>

#include "../../../include/ai/chat/ChatWidgetSerialization.h"
#include "../../../include/ai/edit/EditSessionWidget.h"

#include <QCheckBox>

namespace {

QString sanitizeGeneratedText(const QString &raw, bool isShortFragment) {
  QString text = raw;

  static const QRegularExpression fenceRe(
      QStringLiteral("^\\s*```[a-zA-Z0-9_-]*\\s*\\n?"),
      QRegularExpression::MultilineOption);

  static const QRegularExpression fenceEndRe(
      QStringLiteral("\\n?\\s*```\\s*$"),
      QRegularExpression::MultilineOption);

  text.remove(fenceRe);
  text.remove(fenceEndRe);

  if (isShortFragment) {
    text = text.trimmed();

    static const QRegularExpression dashWrap(
        QStringLiteral("^\\s*---+\\s*(.*?)\\s*---+\\s*$"),
        QRegularExpression::DotMatchesEverythingOption);

    const auto m = dashWrap.match(text);
    if (m.hasMatch()) {
      text = m.captured(1).trimmed();
    }

    if (text.size() >= 2 && text.startsWith(QChar('"')) &&
        text.endsWith(QChar('"'))) {
      text = text.mid(1, text.size() - 2);
    }

    if (text.contains(QChar('\n'))) {
      const QStringList lines = text.split(QChar('\n'));
      for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
          text = trimmed;
          break;
        }
      }
    }
  }

  return text;
}

} // namespace

ChatWidgetEditFlow::ChatWidgetEditFlow(ChatWidget *widget)
    : QObject(widget), m_widget(widget) {}

void ChatWidgetEditFlow::sendPrompt(const QString &prompt) {
  sendPromptWithMode(prompt, 0);
}

void ChatWidgetEditFlow::sendPromptWithMode(const QString &prompt,
                                            int scopeMode) {
  m_scopeMode = scopeMode;

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

  // If ChatWidget has an active token, abort that specific request. This
  // is safe because the token identifies the request ChatWidget owns.
  if (!m_widget->m_activeToken.isNull() && m_widget->m_inferenceService) {
    m_widget->m_inferenceService->abortChatRequest(m_widget->m_activeToken);
    m_widget->m_activeToken = InferenceService::RequestToken();
  }

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

  m_widget->sendChatRequestWithToken(messages, QString(), 0.7, 120000);
}

void ChatWidgetEditFlow::requestNextEditCommand() {
  if (!m_widget->m_editPlanner || !m_widget->m_activeEditor ||
      !m_widget->m_awaitingEdit) {
    return;
  }

  m_widget->appendStatusMessage(QObject::tr("Planning edits…"));

  EditPlanner::ScopeMode mode =
      m_scopeMode == 1 ? EditPlanner::ScopeMode::WholeFile
                       : EditPlanner::ScopeMode::Scoped;

  m_widget->m_editPlanner->start(m_widget->m_activeEditor,
                                 m_widget->m_currentEditRequest, mode);
}

void ChatWidgetEditFlow::onPlanValidated(
    const QVector<EditCommand> &commands) {
  if (!m_widget->m_awaitingEdit) {
    return;
  }

  m_widget->m_plannedEdits =
      QList<EditCommand>(commands.begin(), commands.end());

  m_widget->m_editPhase = ChatWidget::EditPhase::PlanReview;

  m_widget->m_editSessionWidget->clearHistory();

  for (int i = 0; i < m_widget->m_plannedEdits.size(); ++i) {
    const EditCommand &command = m_widget->m_plannedEdits.at(i);

    const int number = i + 1;

    m_widget->m_editSessionWidget->startEdit(number, command.instruction);
    m_widget->m_editSessionWidget->setPlanCommand(number, command);
  }

  m_widget->m_editSessionWidget->showPlanApprovalBar();

  m_widget->appendStatusMessage(
      QObject::tr("Plan ready. Review and approve to proceed."));
}

void ChatWidgetEditFlow::onPlanApprovalRequested(
    const QVector<EditCommand> &editedCommands) {
  if (!m_widget->m_awaitingEdit || !m_widget->m_editSession) {
    return;
  }

  if (editedCommands.isEmpty()) {
    m_widget->appendStatusMessage(QObject::tr("Plan was emptied."));

    resetState();
    return;
  }

  m_widget->m_plannedEdits =
      QList<EditCommand>(editedCommands.begin(), editedCommands.end());

  m_widget->m_editPhase = ChatWidget::EditPhase::Content;

  if (!m_widget->m_editSession->executePlan(editedCommands)) {
    resetState();
  }
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
    ++m_widget->m_nextPlannedEditIndex;
    executeNextPlannedEdit();
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

  const int index = m_widget->m_currentEditNumber - 1;

  if (index < 0 || index >= m_widget->m_plannedEdits.size()) {
    return;
  }

  const EditCommand command = m_widget->m_plannedEdits.at(index);

  const QString instruction = command.instruction.trimmed();

  const QString description = m_widget->m_currentCommandDescription.trimmed();

  const QString findString = command.findString;

  const bool isScopedReplacement =
      command.operation == EditCommand::Operation::Replace;

  const bool isScopeBodyReplacement =
      command.operation == EditCommand::Operation::ReplaceScope;

  QString systemPrompt;
  QString userPrompt;

  if (isScopedReplacement) {
    systemPrompt =
        QStringLiteral(
            "You are a text-substitution engine. The application will replace "
            "exactly one occurrence of a target substring in a document with "
            "your output.\n"
            "\n"
            "Rules:\n"
            "- Output ONLY the replacement for the target substring.\n"
            "- Output must be a single line. No newlines.\n"
            "- Do NOT include the target substring in your output.\n"
            "- Do NOT repeat any word from the surrounding sentence.\n"
            "- Do NOT wrap your output in backticks, quotes, ---, or any "
            "other delimiter.\n"
            "- Do NOT explain your output.\n"
            "- If the instruction is 'replace X with Y', output exactly Y.\n"
            "- If the instruction is 'rename X to Y', output exactly Y.\n"
            "- If the instruction is 'change the label to Y', output exactly "
            "Y.\n"
            "\n"
            "Examples:\n"
            "  Target: \"User\"  Instruction: replace with \"Customer\"  "
            "Output: Customer\n"
            "  Target: \"example\"  Instruction: change to \"illustration\"  "
            "Output: illustration\n"
            "  Target: \"Parse\"  Instruction: rename to \"Transform\"  "
            "Output: Transform\n");
  } else if (isScopeBodyReplacement) {
    systemPrompt =
        QStringLiteral(
            "You are a text generator. The application will replace the "
            "entire body of a section or scope with your output.\n"
            "\n"
            "Rules:\n"
            "- Output ONLY the new body content for the scope.\n"
            "- Do NOT include the scope's heading or title line.\n"
            "- Do NOT wrap your output in backticks or fences.\n"
            "- Do NOT explain your output.\n"
            "- End your output with exactly one trailing newline.\n");
  } else {
    systemPrompt =
        QStringLiteral(
            "You are an automated text generator. Output only the requested "
            "content. No commentary. No markdown fences. No explanation.");
  }

  if (isScopedReplacement) {
    userPrompt =
        QStringLiteral(
            "Target substring (your output replaces exactly this text):\n"
            "%1\n"
            "\n"
            "Instruction:\n%2\n"
            "\n"
            "Output the replacement now. Nothing else.")
            .arg(findString, instruction);
  } else if (isScopeBodyReplacement) {
    userPrompt =
        QStringLiteral(
            "Target scope:\n%1\n"
            "\n"
            "Instruction:\n%2\n"
            "\n"
            "Output the new body now.")
            .arg(description, instruction);
  } else {
    userPrompt =
        QStringLiteral("Generate replacement content.\n\n"
                       "Instruction:\n%1\n\n"
                       "Target:\n%2")
            .arg(instruction, description);
  }

  QJsonArray messages;

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("system")},
      {QStringLiteral("content"), systemPrompt}});

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("user")},
      {QStringLiteral("content"), userPrompt}});

  m_widget->m_editPhase = ChatWidget::EditPhase::Content;

  m_widget->m_currentLlmResponse.clear();

  m_widget->sendChatRequestWithToken(messages, QString(), 0.7, 120000);
}

void ChatWidgetEditFlow::onPlanReady(const QVector<EditCommand> &commands) {
  m_widget->m_plannedEdits =
      QList<EditCommand>(commands.begin(), commands.end());

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

  ++m_widget->m_nextPlannedEditIndex;
  executeNextPlannedEdit();
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
  if (m_widget->m_currentEditNumber > 0) {
    ++m_widget->m_nextPlannedEditIndex;
    executeNextPlannedEdit();
  }
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

  if (m_widget->m_editSessionWidget) {
    m_widget->m_editSessionWidget->hidePlanApprovalBar();
  }
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

  case EditCommand::Operation::ReplaceScope:
    operation = QStringLiteral("Replace Scope");
    break;

  case EditCommand::Operation::Delete:
    operation = QStringLiteral("Delete");
    break;

  case EditCommand::Operation::Unknown:
    operation = QStringLiteral("Unknown");
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

  case EditCommand::Position::Inside:
    position = QStringLiteral("inside");
    break;
  }

  return QStringLiteral("%1 %2 scope %3")
      .arg(operation, position, command.scopeId);
}