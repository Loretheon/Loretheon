#include "../../../include/ai/chat/ChatWidget.h"

#include "../../../include/ai/chat/ChatWidgetEditFlow.h"
#include "../../../include/ai/chat/ChatWidgetLayout.h"
#include "../../../include/ai/chat/ChatWidgetTranscript.h"

#include "../../../include/ai/context/ContextPanel.h"
#include "../../../include/ai/edit/EditSessionWidget.h"
#include "../../../include/ai/history/HistoryModel.h"
#include "../../../include/ai/history/HistoryPanel.h"
#include "NotificationManager.h"

#include "edit/EditPlanner.h"
#include "edit/EditSession.h"

#include "inference/InferenceService.h"

#include "../text/TextEdit.h"
#include "TextDocument.h"

#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>

#include <functional>

namespace {

QJsonObject commandToPlanJson(const EditCommand &command) {
  QString operation;

  switch (command.operation) {
  case EditCommand::Operation::Insert:
    operation = QStringLiteral("insert");
    break;

  case EditCommand::Operation::Replace:
    operation = QStringLiteral("replace");
    break;

  case EditCommand::Operation::ReplaceScope:
    operation = QStringLiteral("replace_scope");
    break;

  case EditCommand::Operation::Delete:
    operation = QStringLiteral("delete");
    break;

  case EditCommand::Operation::Unknown:
    operation = QStringLiteral("unknown");
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

  return QJsonObject{
      {QStringLiteral("operation"), operation},
      {QStringLiteral("scope"), command.scopeId},
      {QStringLiteral("position"), position},
      {QStringLiteral("find"), command.findString},
      {QStringLiteral("all"), command.replaceAll},
      {QStringLiteral("instruction"), command.instruction}};
}

QJsonArray commandsToPlanJson(const QVector<EditCommand> &commands) {
  QJsonArray array;

  for (const EditCommand &command : commands) {
    array.append(commandToPlanJson(command));
  }

  return array;
}

QString firstNonEmptyLine(const DocumentNode &node, const QString &text,
                          int maxChars = 120) {
  const int start = qBound(0, node.start, text.size());
  const int end = qBound(start, node.end, text.size());

  if (end <= start) {
    return {};
  }

  const QStringList lines = text.mid(start, end - start).split(QChar('\n'));

  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();

    if (!trimmed.isEmpty()) {
      return trimmed.left(maxChars);
    }
  }

  return {};
}

QString labelForNode(const DocumentNode &node, const QString &text) {
  if (node.id == QStringLiteral("document")) {
    return QStringLiteral("(document)");
  }

  if (node.type == QStringLiteral("section")) {
    const int start = qBound(0, node.start, text.size());
    const int end = qBound(start, node.end, text.size());

    if (end > start) {
      const QStringList lines = text.mid(start, end - start).split(QChar('\n'));

      for (const QString &line : lines) {
        const QString trimmed = line.trimmed();

        if (trimmed.startsWith(QChar('#'))) {
          return trimmed;
        }
      }
    }
  }

  return firstNonEmptyLine(node, text);
}

} // namespace

ChatWidget::ChatWidget(InferenceService *inferenceService,
                       EditSession *editSession, QWidget *parent)
    : QWidget(parent), m_inferenceService(inferenceService),
      m_editSession(editSession) {
  m_notifications = new NotificationManager(this);

  m_contextModel = new ContextModel(this);

  HistoryModel *historyModel =
      m_editSession ? m_editSession->historyModel() : nullptr;

  ChatWidgetLayout layout;
  layout.build(this, m_contextModel, historyModel);

  m_transcript = layout.transcript;
  m_input = layout.input;
  m_sendButton = layout.sendButton;
  m_editModeCheckbox = layout.editModeCheckbox;
  m_editModeCombo = layout.editModeCombo;
  m_editSessionWidget = layout.editSessionWidget;
  m_contextPanel = layout.contextPanel;
  m_historyPanel = layout.historyPanel;
  m_layout = layout.rootLayout;

  setLayout(m_layout);

  m_editFlow = new ChatWidgetEditFlow(this);

  if (m_inferenceService) {
    m_editPlanner = new EditPlanner(m_inferenceService, this);

    connect(m_editPlanner, &EditPlanner::planValidated, this,
            &ChatWidget::onPlanValidated);

    connect(m_editPlanner, &EditPlanner::failed, m_editFlow,
            &ChatWidgetEditFlow::onPlanFailed);

    connect(m_editPlanner, &EditPlanner::contextScopes, this,
            [this](const QStringList &scopeIds) {
              emit contextScopesChanged(scopeIds);
            });
  }

  connect(m_sendButton, &QPushButton::clicked, this,
          &ChatWidget::onSendClicked);

  connect(m_input, &QLineEdit::returnPressed, this, &ChatWidget::onSendClicked);

  if (m_inferenceService) {
    // Every llm* handler filters on the token issued by the most recent
    // sendChatRequestWithToken call from this widget.
    connect(m_inferenceService, &InferenceService::llmDelta, this,
            [this](const InferenceService::RequestToken &token,
                   const QString &text) {
              if (token != m_activeToken) {
                return;
              }

              if (m_editPhase == EditPhase::Content && m_editSession &&
                  m_editSession->state() == EditSession::State::Streaming) {
                m_editSession->appendStreaming(text);
                return;
              }

              appendAssistantChunk(text);
            });

    connect(m_inferenceService, &InferenceService::llmFinished, this,
            [this](const InferenceService::RequestToken &token) {
              if (token != m_activeToken) {
                return;
              }

              m_activeToken = InferenceService::RequestToken();

              if (m_editPhase == EditPhase::Content && m_editSession &&
                  m_editSession->state() == EditSession::State::Streaming) {
                m_editSession->finishStreaming();
                return;
              }

              renderLastAssistantMessage();
            });

    connect(m_inferenceService, &InferenceService::llmToolCalls, this,
            [this](const InferenceService::RequestToken &token,
                   const QJsonArray &) {
              if (token != m_activeToken) {
                return;
              }

              m_activeToken = InferenceService::RequestToken();
              // ChatWidget does not use tool calls itself; if the model
              // sent them, treat it as an error state for this widget.
              appendStatusMessage(tr("Unexpected tool call in chat mode."));
            });

    connect(m_inferenceService, &InferenceService::llmError, this,
            [this](const InferenceService::RequestToken &token,
                   const QString &error) {
              if (token != m_activeToken) {
                return;
              }

              m_activeToken = InferenceService::RequestToken();
              appendStatusMessage(tr("Error: %1").arg(error));
            });
  }

  if (m_editSession) {
    connect(m_editSession, &EditSession::candidatesReady, m_editFlow,
            &ChatWidgetEditFlow::onEditCandidatesReady);

    connect(m_editSession, &EditSession::applied, m_editFlow,
            &ChatWidgetEditFlow::onEditApplied);

    connect(m_editSession, &EditSession::failed, m_editFlow,
            &ChatWidgetEditFlow::onEditFailed);

    connect(m_editSession, &EditSession::aborted, m_editFlow,
            &ChatWidgetEditFlow::onEditAborted);

    connect(m_editSession, &EditSession::planValidated, this,
            &ChatWidget::onPlanValidatedFromSession);

    connect(m_editSession, &EditSession::planReady, this,
            &ChatWidget::onPlanReady);

    connect(m_editSession, &EditSession::reviewReady, m_editFlow,
            &ChatWidgetEditFlow::onReviewReady);

    connect(m_editSession, &EditSession::pendingEditStarted, this,
            &ChatWidget::onPendingEditStarted);

    connect(m_editSession, &EditSession::pendingEditUpdated, this,
            &ChatWidget::onPendingEditUpdated);

    connect(m_editSession, &EditSession::pendingEditFinished, this,
            &ChatWidget::onPendingEditFinished);

    connect(m_editSession, &EditSession::pendingEditsApplied, this,
            [this](const QStringList &scopeIds) {
              if (m_contextModel) {
                m_contextModel->markSent(expandSentScopes(scopeIds));
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

  connect(m_editSessionWidget, &EditSessionWidget::planApprovalRequested, this,
          &ChatWidget::onPlanApprovalRequested);

  connect(m_editSessionWidget, &EditSessionWidget::planCancelled, this,
          &ChatWidget::onPlanCancelled);
}

void ChatWidget::setActiveEditor(TextEdit *editor) {
  if (m_activeEditor) {
    if (auto *doc = m_activeEditor->document()) {
      disconnect(doc, &QTextDocument::contentsChanged, this,
                 &ChatWidget::onDocumentStructureChanged);
    }
  }

  m_activeEditor = editor;

  if (m_editSession) {
    m_editSession->setEditor(editor);
  }

  if (m_activeEditor) {
    if (auto *doc = m_activeEditor->document()) {
      connect(doc, &QTextDocument::contentsChanged, this,
              &ChatWidget::onDocumentStructureChanged);
    }

    m_activeEditor->refreshPendingEdits();
  }

  if (m_contextModel) {
    m_contextModel->clear();
  }

  refreshContextModel();
}

void ChatWidget::onDocumentStructureChanged() {
  refreshContextModel();
}

QStringList ChatWidget::expandSentScopes(const QStringList &scopeIds) const {
  if (!m_activeEditor) {
    return scopeIds;
  }

  auto *doc = qobject_cast<TextDocument *>(m_activeEditor->document());

  if (!doc) {
    return scopeIds;
  }

  const DocumentStructure &structure = doc->structure();

  bool wholeDocument = scopeIds.contains(QStringLiteral("document"));

  QVector<QPair<int, int>> ranges;

  if (!wholeDocument) {
    for (const QString &id : scopeIds) {
      const DocumentNode *node = structure.find(id);

      if (node) {
        ranges.append({node->start, node->end});
      }
    }
  }

  QStringList expanded;

  std::function<void(const DocumentNode &)> walk =
      [&](const DocumentNode &node) {
        if (!node.id.isEmpty()) {
          bool include = wholeDocument;

          if (!include) {
            for (const auto &range : ranges) {
              if (node.start >= range.first && node.end <= range.second) {
                include = true;
                break;
              }
            }
          }

          if (include && !expanded.contains(node.id)) {
            expanded.append(node.id);
          }
        }

        for (const DocumentNode &child : node.children) {
          walk(child);
        }
      };

  walk(structure.root());

  return expanded;
}

void ChatWidget::refreshContextModel() {
  if (!m_contextModel) {
    return;
  }

  if (!m_activeEditor) {
    m_contextModel->clear();
    return;
  }

  auto *doc = qobject_cast<TextDocument *>(m_activeEditor->document());

  if (!doc) {
    m_contextModel->clear();
    return;
  }

  doc->rebuildStructure();

  const DocumentStructure &structure = doc->structure();

  const QString documentText = doc->toPlainText();

  QVector<ContextModel::Entry> entries;

  std::function<void(const DocumentNode &, int)> walk =
      [&](const DocumentNode &node, int depth) {
        const bool isRoot = node.id == QStringLiteral("document");

        if (!isRoot && node.id.isEmpty()) {
        } else {
          ContextModel::Entry entry;

          entry.scopeId = node.id;
          entry.depth = depth;
          entry.contentHash = node.contentHash;
          entry.heading = labelForNode(node, documentText);

          entries.append(entry);
        }

        for (const DocumentNode &child : node.children) {
          walk(child, depth + 1);
        }
      };

  walk(structure.root(), 0);

  QHash<QString, QString> hashes;
  QHash<QString, QString> headings;

  for (const ContextModel::Entry &entry : entries) {
    hashes.insert(entry.scopeId, entry.contentHash);
    headings.insert(entry.scopeId, entry.heading);
  }

  if (m_contextModel->entries().isEmpty()) {
    m_contextModel->setScopes(entries);
  } else {
    m_contextModel->refreshContentHashes(hashes, headings);

    bool needsRebuild = false;

    if (m_contextModel->entries().size() != entries.size()) {
      needsRebuild = true;
    } else {
      for (int i = 0; i < entries.size(); ++i) {
        if (m_contextModel->entries().at(i).scopeId != entries.at(i).scopeId) {
          needsRebuild = true;
          break;
        }
      }
    }

    if (needsRebuild) {
      m_contextModel->setScopes(entries);
    }
  }

  if (documentText.isEmpty()) {
    for (const ContextModel::Entry &entry : m_contextModel->entries()) {
      if (entry.scopeId == QStringLiteral("document")) {
        m_contextModel->setIncluded(QStringLiteral("document"), true);
        break;
      }
    }
  }
}

void ChatWidget::submitTranscribedText(const QString &text) {
  if (text.trimmed().isEmpty()) {
    return;
  }

  m_input->setText(text);

  sendPrompt(text);
}

void ChatWidget::triggerWholeFileRewrite() {
  const QString prompt = m_input->text().trimmed();

  if (prompt.isEmpty()) {
    appendStatusMessage(
        tr("Type a rewrite instruction in the input box first."));
    return;
  }

  m_input->clear();

  sendPromptWithMode(prompt, 1);
}

void ChatWidget::onSendClicked() {
  const QString prompt = m_input->text().trimmed();

  if (prompt.isEmpty()) {
    return;
  }

  m_input->clear();

  int scopeMode = 0;

  if (m_editModeCombo) {
    scopeMode = m_editModeCombo->currentIndex();
  }

  sendPromptWithMode(prompt, scopeMode);
}

void ChatWidget::sendPrompt(const QString &prompt) {
  sendPromptWithMode(prompt, 0);
}

void ChatWidget::sendPromptWithMode(const QString &prompt, int scopeMode) {
  if (m_editFlow) {
    m_editFlow->sendPromptWithMode(prompt, scopeMode);
  }
}

InferenceService::RequestToken ChatWidget::sendChatRequestWithToken(
    const QJsonArray &messages, const QString &model, double temperature,
    int timeoutMs) {
  if (!m_inferenceService) {
    return InferenceService::RequestToken();
  }

  m_activeToken = m_inferenceService->sendChatRequest(
      messages, model, temperature, timeoutMs);

  return m_activeToken;
}

void ChatWidget::appendUserMessage(const QString &text) {
  ChatWidgetTranscript::appendUserMessage(m_transcript, text);

  m_assistantMessageOpen = false;
}

void ChatWidget::appendAssistantChunk(const QString &text) {
  ChatWidgetTranscript::appendAssistantChunk(m_transcript, text,
                                             m_assistantMessageOpen);
}

void ChatWidget::appendStatusMessage(const QString &text) {
  ChatWidgetTranscript::appendStatusMessage(m_transcript, text);

  m_assistantMessageOpen = false;
}

void ChatWidget::renderLastAssistantMessage() {
  ChatWidgetTranscript::renderLastAssistantMessage(m_transcript);
}

void ChatWidget::onPendingEditStarted(const PendingEdit &edit) {
  if (!m_editSessionWidget) {
    return;
  }

  m_editSessionWidget->startEdit(edit.id, edit.command.instruction);

  m_editSessionWidget->setStatus(edit.id, tr("Writing"));
}

void ChatWidget::onPendingEditUpdated(const PendingEdit &edit) {
  if (!m_editSessionWidget) {
    return;
  }

  m_editSessionWidget->setResultText(edit.id, edit.generatedText);
}

void ChatWidget::onPendingEditFinished(const PendingEdit &edit) {
  if (!m_editSessionWidget) {
    return;
  }

  m_editSessionWidget->setResultText(edit.id, edit.generatedText);

  m_editSessionWidget->setPendingEditReady(edit.id);
}

void ChatWidget::onPlanValidated(const QVector<EditCommand> &commands) {
  if (!m_editSession) {
    appendStatusMessage(tr("Edit session is unavailable."));
    return;
  }

  const QJsonArray planArray = commandsToPlanJson(commands);

  if (!m_editSession->validatePlan(planArray)) {
    return;
  }

  if (m_contextModel) {
    QStringList referenced;

    for (const EditCommand &command : commands) {
      if (command.scopeId.isEmpty()) {
        continue;
      }

      if (!referenced.contains(command.scopeId)) {
        referenced.append(command.scopeId);
      }
    }

    if (!referenced.isEmpty()) {
      m_contextModel->markSent(expandSentScopes(referenced));
    }
  }
}

void ChatWidget::onPlanValidatedFromSession(
    const QVector<EditCommand> &commands) {
  if (m_editFlow) {
    m_editFlow->onPlanValidated(commands);
  }
}

void ChatWidget::onPlanApprovalRequested(
    const QVector<EditCommand> &commands) {
  if (m_editFlow) {
    m_editFlow->onPlanApprovalRequested(commands);
  }
}

void ChatWidget::onPlanCancelled() {
  appendStatusMessage(tr("Plan cancelled."));

  emit previewActivationRequested(false);

  if (m_editSession) {
    m_editSession->abort();
  }

  if (m_editFlow) {
    m_editFlow->resetState();
  }
}

void ChatWidget::onPlanReady(const QVector<EditCommand> &commands) {
  if (m_editFlow) {
    m_editFlow->onPlanReady(commands);
  }

  emit previewActivationRequested(true);
}

void ChatWidget::onPlanFailed(const QString &reason) {
  if (m_editFlow) {
    m_editFlow->onPlanFailed(reason);
  }
}

void ChatWidget::onPendingEditAccepted(int index) {
  if (m_editSession) {
    m_editSession->acceptPendingEdit(index);
  }
}

void ChatWidget::onPendingEditRejected(int index) {
  if (m_editSession) {
    m_editSession->rejectPendingEdit(index);
  }
}

void ChatWidget::onAcceptAllPendingEdits() {
  if (m_editSession) {
    m_editSession->acceptAllPendingEdits();
  }
}

void ChatWidget::onRejectAllPendingEdits() {
  if (m_editSession) {
    m_editSession->rejectAllPendingEdits();
  }
}

void ChatWidget::onApplyAcceptedPendingEdits() {
  if (m_editSession) {
    m_editSession->applyAcceptedPendingEdits();
  }

  emit previewActivationRequested(false);

  emit contextScopesChanged({});
}

void ChatWidget::onEditCandidatesReady(
    const QVector<EditMatch> &candidates) {
  if (m_editFlow) {
    m_editFlow->onEditCandidatesReady(candidates);
  }
}

void ChatWidget::onEditApplied(bool fuzzy, int distance) {
  if (m_editFlow) {
    m_editFlow->onEditApplied(fuzzy, distance);
  }
}

void ChatWidget::onEditFailed(const QString &reason) {
  if (m_editFlow) {
    m_editFlow->onEditFailed(reason);
  }
}

void ChatWidget::onEditAborted() {
  emit previewActivationRequested(false);

  if (m_editFlow) {
    m_editFlow->onEditAborted();
  }
}

void ChatWidget::onReviewReady() {
  if (m_editFlow) {
    m_editFlow->onReviewReady();
  }
}

void ChatWidget::onConflictsDetected() {}

void ChatWidget::onConflictResolved(int, int) {}

void ChatWidget::onConflictGroupDiscarded(int) {}

void ChatWidget::onConflictBatchAborted() {}

void ChatWidget::resetEditState() {
  m_plannedEdits.clear();

  m_nextPlannedEditIndex = 0;
  m_currentEditNumber = 0;
  m_streamingEditCount = 0;

  m_currentEditInstruction.clear();
  m_currentCommandDescription.clear();
  m_currentCommandJson.clear();
  m_currentLlmResponse.clear();

  m_awaitingEdit = false;
  m_editGenerationStopped = false;
  m_editAbortRequested = false;
  m_planReadyToStream = false;

  m_editPhase = EditPhase::None;
}