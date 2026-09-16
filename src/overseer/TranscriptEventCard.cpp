#include "../../include/overseer/TranscriptEventCard.h"

#include "../../include/app/theme/ThemeTokens.h"
#include "../../include/overseer/MarkdownView.h"

#include <QClipboard>
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

QString roleForEvent(TranscriptEvent::Type type, bool toolOk) {
  switch (type) {
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
  m_event = event;
  refreshStatusDot();
  update();
}

void TranscriptEventCard::refreshStatusDot() {
  setStatusDot(
      ThemeRegistry::instance()
          .color(roleForEvent(m_event.type, m_event.toolOk))
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
  case TranscriptEvent::Type::UserMessage:
    return tr("You");
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
  case TranscriptEvent::Type::MemoryProposal:
    return tr("Memory proposal");
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
  auto *fact = new QLabel(m_event.proposalFact, this);
  fact->setWordWrap(true);
  fact->setObjectName(QStringLiteral("transcriptProposalFact"));
  bodyLayout->addWidget(fact);

  if (!m_event.proposalRationale.isEmpty()) {
    auto *rationale = new QLabel(m_event.proposalRationale, this);
    rationale->setWordWrap(true);
    rationale->setObjectName(QStringLiteral("transcriptProposalRationale"));
    bodyLayout->addWidget(rationale);
  }

  auto *row = new QHBoxLayout;

  const bool pending = m_event.proposalStatus.isEmpty() ||
                       m_event.proposalStatus == QStringLiteral("pending");

  if (pending) {
    auto *accept = new QPushButton(QStringLiteral("\u2713"), this);
    auto *reject = new QPushButton(QStringLiteral("\u2717"), this);

    connect(accept, &QPushButton::clicked, this, [this]() {
      emit memoryProposalAccepted(m_event.proposalKey);
    });
    connect(reject, &QPushButton::clicked, this, [this]() {
      emit memoryProposalRejected(m_event.proposalKey);
    });

    row->addWidget(accept);
    row->addWidget(reject);
    row->addStretch(1);
  } else {
    auto *status = new QLabel(this);
    status->setObjectName(QStringLiteral("transcriptProposalStatus"));
    status->setText(m_event.proposalStatus == QStringLiteral("accepted")
                        ? tr("\u2713 Accepted")
                        : tr("\u2717 Rejected"));
    row->addWidget(status);
    row->addStretch(1);
  }

  bodyLayout->addLayout(row);
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

    QAction *accept = menu->addAction(tr("Accept proposal"));
    QAction *reject = menu->addAction(tr("Reject proposal"));

    connect(accept, &QAction::triggered, this, [this]() {
      emit memoryProposalAccepted(m_event.proposalKey);
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

  return menu;
}