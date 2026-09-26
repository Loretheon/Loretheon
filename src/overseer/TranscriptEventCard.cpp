#include "../../include/overseer/TranscriptEventCard.h"

#include "../../include/app/theme/ThemeTokens.h"
#include "../../include/overseer/MarkdownView.h"
#include "../../include/overseer/TranscriptEditPlanCard.h"
#include "ThemeRegistry.h"

#include <QClipboard>
#include <QComboBox>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString formatTimestamp(const QDateTime &ts) {
  return ts.toString(QStringLiteral("HH:mm:ss"));
}

QString roleForEvent(TranscriptEvent::Type type, Origin origin,
                     bool toolOk) {
  if (origin == Origin::Lore &&
      type == TranscriptEvent::Type::UserMessage) {
    return QStringLiteral("event.lore");
  }

  switch (type) {
  case TranscriptEvent::Type::EditPlan:
    return QStringLiteral("event.proposal");
  case TranscriptEvent::Type::UserMessage:
    return QStringLiteral("event.user");
  case TranscriptEvent::Type::AssistantMessage:
    return QStringLiteral("event.assistant");
  case TranscriptEvent::Type::ToolCall:
  case TranscriptEvent::Type::ToolResult:
    return toolOk ? QStringLiteral("event.tool.ok")
                  : QStringLiteral("event.tool.error");
  case TranscriptEvent::Type::MemoryProposal:
    return QStringLiteral("event.proposal");
  case TranscriptEvent::Type::Stage:
    return QStringLiteral("event.stage");
  case TranscriptEvent::Type::Promotion:
    return QStringLiteral("event.promotion");
  case TranscriptEvent::Type::Error:
    return QStringLiteral("event.error");
  case TranscriptEvent::Type::Notice:
    return QStringLiteral("event.notice");
  }
  return QStringLiteral("event.notice");
}

QString scopeLabelFor(const QString &scope) {
  if (scope == QStringLiteral("global"))
    return QObject::tr("global");
  if (scope == QStringLiteral("session"))
    return QObject::tr("session");
  return {};
}

} // namespace

TranscriptEventCard::TranscriptEventCard(const TranscriptEvent &event,
                                         QWidget *parent)
    : CardWidget(parent), m_event(event) {
  setTitle(headerTitleFor(event));

  const QString ts = formatTimestamp(event.timestamp);

  QString sub = ts;

  if (event.toolDurationMs > 0)
    sub += QStringLiteral(" · %1ms").arg(event.toolDurationMs);
  if (event.outputTokens > 0)
    sub += QStringLiteral(" · %1 tok").arg(event.outputTokens);

  setSubtitle(sub);

  refreshStatusDot();

  buildBody();
}

void TranscriptEventCard::updateEvent(const TranscriptEvent &event) {
  const bool proposalChanged =
      event.proposalStatus != m_event.proposalStatus ||
      event.proposalAcceptedScope != m_event.proposalAcceptedScope;

  m_event = event;

  setTitle(headerTitleFor(event));
  refreshStatusDot();

  if (proposalChanged) {
    refreshProposalControls();
  }

  if (m_planCard && event.type == TranscriptEvent::Type::EditPlan) {
    m_planCard->setPlanStatus(event.planStatus, event.planResult);
  }

  update();
}

void TranscriptEventCard::refreshStatusDot() {
  setStatusDot(
      ThemeRegistry::instance()
          .color(roleForEvent(m_event.type, m_event.origin, m_event.toolOk))
          .name());
}

void TranscriptEventCard::changeEvent(QEvent *event) {
  if (event && (event->type() == QEvent::PaletteChange ||
                event->type() == QEvent::StyleChange)) {
    refreshStatusDot();
    update();
  }
  CardWidget::changeEvent(event);
}

QString TranscriptEventCard::headerTitleFor(const TranscriptEvent &event) const {
  switch (event.type) {
  case TranscriptEvent::Type::EditPlan:
    return tr("Edit plan");
  case TranscriptEvent::Type::UserMessage:
    return event.origin == Origin::Lore ? tr("Lore") : tr("You");
  case TranscriptEvent::Type::AssistantMessage:
    return tr("Overseer");
  case TranscriptEvent::Type::ToolCall:
  case TranscriptEvent::Type::ToolResult:
    if (!event.toolName.isEmpty()) {
      const QString mark = event.toolOk ? QStringLiteral("\u2713")
                                        : QStringLiteral("\u2717");
      return QStringLiteral("%1  %2").arg(mark, event.toolName);
    }
    return tr("Tool");
  case TranscriptEvent::Type::MemoryProposal: {
    const QString scope = scopeLabelFor(event.proposalScope);

    QString title;

    if (event.proposalFact.isEmpty() && !event.proposalContext.isEmpty())
      title = tr("Memory deletion");
    else if (!event.proposalContext.isEmpty())
      title = tr("Memory edit");
    else
      title = tr("Memory proposal");

    return scope.isEmpty() ? title : tr("%1 · %2").arg(title, scope);
  }
  case TranscriptEvent::Type::Stage:
    return tr("Staged file");
  case TranscriptEvent::Type::Promotion:
    return tr("Promoted file");
  case TranscriptEvent::Type::Error:
    return tr("Error");
  case TranscriptEvent::Type::Notice:
    return tr("Notice");
  }
  return tr("Event");
}

void TranscriptEventCard::populateBody(QVBoxLayout *bodyLayout) {
  switch (m_event.type) {
  case TranscriptEvent::Type::UserMessage:
  case TranscriptEvent::Type::AssistantMessage:
    buildUserAssistant(m_event.type, bodyLayout);
    break;
  case TranscriptEvent::Type::ToolCall:
    buildToolCall(bodyLayout);
    break;
  case TranscriptEvent::Type::ToolResult:
    buildToolResult(bodyLayout);
    break;
  case TranscriptEvent::Type::MemoryProposal:
    buildProposal(bodyLayout);
    break;
  case TranscriptEvent::Type::EditPlan:
    buildEditPlan(bodyLayout);
    break;
  case TranscriptEvent::Type::Stage:
    buildStage(bodyLayout);
    break;
  case TranscriptEvent::Type::Error:
    buildError(bodyLayout);
    break;
  case TranscriptEvent::Type::Promotion:
  case TranscriptEvent::Type::Notice:
    buildNotice(bodyLayout);
    break;
  }
}

void TranscriptEventCard::buildEditPlan(QVBoxLayout *bodyLayout) {
  m_planCard = new TranscriptEditPlanCard(
      m_event.planId, m_event.planFilePath, m_event.planInstruction,
      m_event.planCommands, m_event.planStatus, m_event.planResult, this);

  connect(m_planCard, &TranscriptEditPlanCard::editAccepted, this,
          &TranscriptEventCard::planEditAccepted);

  connect(m_planCard, &TranscriptEditPlanCard::editRejected, this,
          &TranscriptEventCard::planEditRejected);

  connect(m_planCard, &TranscriptEditPlanCard::applyRequested, this,
          &TranscriptEventCard::planApplyRequested);

  connect(m_planCard, &TranscriptEditPlanCard::cancelRequested, this,
          &TranscriptEventCard::planCancelRequested);

  bodyLayout->addWidget(m_planCard);
}

void TranscriptEventCard::buildUserAssistant(TranscriptEvent::Type type,
                                              QVBoxLayout *bodyLayout) {
  m_bodyView = new MarkdownView(this);
  m_bodyView->setObjectName(
      type == TranscriptEvent::Type::UserMessage
          ? QStringLiteral("transcriptUserBody")
          : QStringLiteral("transcriptAssistantBody"));

  m_bodyView->setMarkdownText(m_event.body);

  bodyLayout->addWidget(m_bodyView);
}

void TranscriptEventCard::buildToolCall(QVBoxLayout *bodyLayout) {
  if (!m_event.body.isEmpty()) {
    m_bodyView = new MarkdownView(this);
    m_bodyView->setObjectName(QStringLiteral("transcriptToolBody"));
    m_bodyView->setMarkdownText(m_event.body);
    bodyLayout->addWidget(m_bodyView);
  }

  if (!m_event.toolArguments.isEmpty()) {
    const QString args = QString::fromUtf8(
        QJsonDocument(m_event.toolArguments).toJson(QJsonDocument::Indented));

    m_bodyEdit = new QPlainTextEdit(this);
    m_bodyEdit->setReadOnly(true);
    m_bodyEdit->setPlainText(args);
    m_bodyEdit->setFixedHeight(120);
    m_bodyEdit->setObjectName(QStringLiteral("transcriptToolArgs"));

    bodyLayout->addWidget(m_bodyEdit);
  }
}

void TranscriptEventCard::buildToolResult(QVBoxLayout *bodyLayout) {
  m_bodyView = new MarkdownView(this);
  m_bodyView->setObjectName(QStringLiteral("transcriptToolResult"));

  const QString text =
      m_event.toolResult.isEmpty() ? m_event.body : m_event.toolResult;

  m_bodyView->setMarkdownText(text);

  bodyLayout->addWidget(m_bodyView);
}

void TranscriptEventCard::buildProposal(QVBoxLayout *bodyLayout) {
  // "replaces" line, above the fact. Shown for edits and deletions.
  if (!m_event.proposalContext.isEmpty()) {
    const QString prefix =
        m_event.proposalFact.isEmpty()
            ? tr("Deletes: %1").arg(m_event.proposalContext)
            : tr("Replaces: %1").arg(m_event.proposalContext);

    auto *replaced = new QLabel(prefix, this);
    replaced->setWordWrap(true);
    replaced->setObjectName(QStringLiteral("transcriptProposalReplaced"));
    replaced->setTextInteractionFlags(Qt::TextSelectableByMouse);

    {
      QFont small = replaced->font();
      small.setPointSizeF(qMax(7.0, small.pointSizeF() - 1.0));
      small.setItalic(true);
      replaced->setFont(small);
    }

    bodyLayout->addWidget(replaced);
  }

  if (!m_event.proposalFact.isEmpty()) {
    auto *fact = new QLabel(m_event.proposalFact, this);
    fact->setWordWrap(true);
    fact->setObjectName(QStringLiteral("transcriptProposalFact"));
    bodyLayout->addWidget(fact);
  }

  if (!m_event.proposalRationale.isEmpty()) {
    auto *rationale = new QLabel(m_event.proposalRationale, this);
    rationale->setWordWrap(true);
    rationale->setObjectName(QStringLiteral("transcriptProposalRationale"));
    bodyLayout->addWidget(rationale);
  }

  m_proposalButtonRow = new QWidget(this);
  auto *row = new QHBoxLayout(m_proposalButtonRow);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(6);

  m_proposalScopeCombo = new QComboBox(m_proposalButtonRow);
  m_proposalScopeCombo->addItem(tr("Global memory"),
                                QStringLiteral("global"));
  m_proposalScopeCombo->addItem(tr("Session memory"),
                                QStringLiteral("session"));

  {
    const int index =
        m_proposalScopeCombo->findData(m_event.proposalScope);
    if (index >= 0)
      m_proposalScopeCombo->setCurrentIndex(index);
  }

  m_proposalAccept =
      new QPushButton(QStringLiteral("\u2713"), m_proposalButtonRow);
  m_proposalReject =
      new QPushButton(QStringLiteral("\u2717"), m_proposalButtonRow);

  m_proposalAccept->setToolTip(tr("Accept into the selected scope"));
  m_proposalReject->setToolTip(tr("Reject this proposal"));

  connect(m_proposalAccept, &QPushButton::clicked, this, [this]() {
    const QString scope = m_proposalScopeCombo
                              ? m_proposalScopeCombo->currentData().toString()
                              : m_event.proposalScope;
    emit memoryProposalAccepted(m_event.proposalKey, scope);
  });

  connect(m_proposalReject, &QPushButton::clicked, this, [this]() {
    emit memoryProposalRejected(m_event.proposalKey);
  });

  m_proposalStatusLabel = new QLabel(m_proposalButtonRow);
  m_proposalStatusLabel->setObjectName(
      QStringLiteral("transcriptProposalStatus"));
  m_proposalStatusLabel->setVisible(false);

  row->addWidget(m_proposalScopeCombo);
  row->addWidget(m_proposalAccept);
  row->addWidget(m_proposalReject);
  row->addWidget(m_proposalStatusLabel);
  row->addStretch(1);

  bodyLayout->addWidget(m_proposalButtonRow);

  refreshProposalControls();
}

void TranscriptEventCard::refreshProposalControls() {
  if (!m_proposalButtonRow)
    return;

  const bool pending = m_event.proposalStatus.isEmpty() ||
                       m_event.proposalStatus == QStringLiteral("pending");

  const bool isDelete =
      m_event.proposalFact.isEmpty() && !m_event.proposalContext.isEmpty();

  // A deletion has a fixed scope. The combo is not shown.
  if (m_proposalScopeCombo)
    m_proposalScopeCombo->setVisible(pending && !isDelete);

  if (m_proposalAccept)
    m_proposalAccept->setVisible(pending);

  if (m_proposalReject)
    m_proposalReject->setVisible(pending);

  if (!m_proposalStatusLabel)
    return;

  if (pending) {
    m_proposalStatusLabel->setVisible(false);
    return;
  }

  const QString scope = scopeLabelFor(m_event.proposalAcceptedScope);

  if (m_event.proposalStatus == QStringLiteral("accepted")) {
    QString text;

    if (isDelete)
      text = tr("\u2713 Deleted");
    else if (!m_event.proposalContext.isEmpty())
      text = scope.isEmpty() ? tr("\u2713 Replaced")
                             : tr("\u2713 Replaced in %1").arg(scope);
    else
      text = scope.isEmpty() ? tr("\u2713 Accepted")
                             : tr("\u2713 Accepted into %1").arg(scope);

    m_proposalStatusLabel->setText(text);
  } else {
    m_proposalStatusLabel->setText(tr("\u2717 Rejected"));
  }

  m_proposalStatusLabel->setVisible(true);
}

void TranscriptEventCard::buildStage(QVBoxLayout *bodyLayout) {
  m_bodyView = new MarkdownView(this);
  m_bodyView->setObjectName(QStringLiteral("transcriptStagePath"));
  m_bodyView->setMarkdownText(QStringLiteral("`%1`").arg(m_event.filePath));

  bodyLayout->addWidget(m_bodyView);
}

void TranscriptEventCard::buildError(QVBoxLayout *bodyLayout) {
  m_bodyView = new MarkdownView(this);
  m_bodyView->setObjectName(QStringLiteral("transcriptErrorBody"));
  m_bodyView->setMarkdownText(m_event.body);

  bodyLayout->addWidget(m_bodyView);
}

void TranscriptEventCard::buildNotice(QVBoxLayout *bodyLayout) {
  if (!m_event.body.isEmpty()) {
    m_bodyView = new MarkdownView(this);
    m_bodyView->setObjectName(QStringLiteral("transcriptNoticeBody"));
    m_bodyView->setMarkdownText(m_event.body);
    bodyLayout->addWidget(m_bodyView);
  }

  if (!m_event.filePath.isEmpty()) {
    auto *path = new MarkdownView(this);
    path->setObjectName(QStringLiteral("transcriptNoticePath"));
    path->setMarkdownText(QStringLiteral("`%1`").arg(m_event.filePath));
    bodyLayout->addWidget(path);
  }
}

QMenu *TranscriptEventCard::buildContextMenu(QWidget *parent) {
  auto *menu = new QMenu(parent);

  QAction *copy = menu->addAction(tr("Copy content"));

  connect(copy, &QAction::triggered, this, [this]() {
    QString text = m_event.body;

    if (m_event.type == TranscriptEvent::Type::ToolCall)
      text = QString::fromUtf8(
          QJsonDocument(m_event.toolArguments).toJson(QJsonDocument::Indented));

    if (m_event.type == TranscriptEvent::Type::ToolResult)
      text = m_event.toolResult;

    if (m_event.type == TranscriptEvent::Type::MemoryProposal)
      text = m_event.proposalFact;

    QGuiApplication::clipboard()->setText(text);
  });

  if (m_event.type == TranscriptEvent::Type::MemoryProposal &&
      (m_event.proposalStatus.isEmpty() ||
       m_event.proposalStatus == QStringLiteral("pending"))) {
    menu->addSeparator();

    QAction *acceptGlobal = menu->addAction(tr("Accept into global memory"));
    QAction *acceptSession = menu->addAction(tr("Accept into session memory"));
    QAction *reject = menu->addAction(tr("Reject proposal"));

    connect(acceptGlobal, &QAction::triggered, this, [this]() {
      emit memoryProposalAccepted(m_event.proposalKey,
                                  QStringLiteral("global"));
    });
    connect(acceptSession, &QAction::triggered, this, [this]() {
      emit memoryProposalAccepted(m_event.proposalKey,
                                  QStringLiteral("session"));
    });
    connect(reject, &QAction::triggered, this, [this]() {
      emit memoryProposalRejected(m_event.proposalKey);
    });
  }

  if (m_event.type == TranscriptEvent::Type::Stage ||
      m_event.type == TranscriptEvent::Type::Promotion) {
    menu->addSeparator();

    QAction *copyPath = menu->addAction(tr("Copy path"));
    connect(copyPath, &QAction::triggered, this, [this]() {
      QGuiApplication::clipboard()->setText(m_event.filePath);
    });
  }

  if (m_event.type == TranscriptEvent::Type::EditPlan &&
      (m_event.planStatus.isEmpty() ||
       m_event.planStatus == QStringLiteral("pending"))) {
    menu->addSeparator();

    QAction *apply = menu->addAction(tr("Apply accepted edits"));
    QAction *cancel = menu->addAction(tr("Cancel plan"));

    connect(apply, &QAction::triggered, this, [this]() {
      emit planApplyRequested(m_event.planId);
    });

    connect(cancel, &QAction::triggered, this, [this]() {
      emit planCancelRequested(m_event.planId);
    });
  }

  return menu;
}