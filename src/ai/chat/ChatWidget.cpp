#include "../../../include/ai/chat/ChatWidget.h"

#include "../../../include/ai/chat/ChatWidgetEditFlow.h"
#include "../../../include/ai/chat/ChatWidgetLayout.h"
#include "../../../include/ai/chat/ChatWidgetTranscript.h"

#include "../../../include/ai/edit/EditSessionWidget.h"
#include "NotificationManager.h"

#include "edit/EditPlanner.h"
#include "edit/EditSession.h"

#include "inference/InferenceService.h"

#include "../text/TextEdit.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>

ChatWidget::ChatWidget(InferenceService *inferenceService,
                       EditSession *editSession, QWidget *parent)
    : QWidget(parent), m_inferenceService(inferenceService),
      m_editSession(editSession) {
  m_notifications = new NotificationManager(this);

  ChatWidgetLayout layout;
  layout.build(this);

  m_transcript = layout.transcript;

  m_input = layout.input;

  m_sendButton = layout.sendButton;

  m_editModeCheckbox = layout.editModeCheckbox;

  m_editSessionWidget = layout.editSessionWidget;

  m_layout = layout.rootLayout;

  setLayout(m_layout);

  m_editFlow = new ChatWidgetEditFlow(this);

  if (m_inferenceService) {
    m_editPlanner = new EditPlanner(m_inferenceService, this);

    connect(m_editPlanner, &EditPlanner::planReady, m_editFlow,
            &ChatWidgetEditFlow::onPlanReady);

    connect(m_editPlanner, &EditPlanner::failed, m_editFlow,
            &ChatWidgetEditFlow::onPlanFailed);
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
    connect(m_editSession, &EditSession::candidatesReady, m_editFlow,
            &ChatWidgetEditFlow::onEditCandidatesReady);

    connect(m_editSession, &EditSession::applied, m_editFlow,
            &ChatWidgetEditFlow::onEditApplied);

    connect(m_editSession, &EditSession::failed, m_editFlow,
            &ChatWidgetEditFlow::onEditFailed);

    connect(m_editSession, &EditSession::aborted, m_editFlow,
            &ChatWidgetEditFlow::onEditAborted);

    connect(m_editSession, &EditSession::planReady, m_editFlow,
            &ChatWidgetEditFlow::onPlanReady);

    connect(m_editSession, &EditSession::reviewReady, m_editFlow,
            &ChatWidgetEditFlow::onReviewReady);
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
  if (m_editFlow) {
    m_editFlow->sendPrompt(prompt);
  }
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

void ChatWidget::onLlmDelta(const QString &delta) {
  appendAssistantChunk(delta);
}

void ChatWidget::onLlmFinished() { renderLastAssistantMessage(); }

void ChatWidget::onLlmError(const QString &error) {
  appendStatusMessage(tr("Error: %1").arg(error));
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
}

void ChatWidget::onPlanReady(const QList<EditCommand> &commands) {
  if (m_editFlow) {
    m_editFlow->onPlanReady(commands.toVector());
  }
}

void ChatWidget::onPlanFailed(const QString &reason) {
  if (m_editFlow) {
    m_editFlow->onPlanFailed(reason);
  }
}

void ChatWidget::onEditCandidatesReady(const QList<EditMatch> &candidates) {
  if (m_editFlow) {
    m_editFlow->onEditCandidatesReady(candidates.toVector());
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