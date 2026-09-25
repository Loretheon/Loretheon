#include "../../include/assistant/ChatNodeWidget.h"

#include "../../include/assistant/ChatTree.h"
#include "../../include/overseer/MarkdownView.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString glyphFor(ChatNode::Kind kind) {
  switch (kind) {
  case ChatNode::Kind::JobSearch:
    return QStringLiteral("⌕");
  case ChatNode::Kind::JobDelegate:
    return QStringLiteral("⇢");
  case ChatNode::Kind::JobPromote:
    return QStringLiteral("★");
  case ChatNode::Kind::JobRead:
    return QStringLiteral("⤓");
  case ChatNode::Kind::Tool:
    return QStringLiteral("⚙");
  case ChatNode::Kind::Status:
    return QStringLiteral("·");
  case ChatNode::Kind::Error:
    return QStringLiteral("!");
  default:
    return QString();
  }
}

QString stateLabel(ChatNode::State state) {
  switch (state) {
  case ChatNode::State::Pending:
    return QObject::tr("queued");
  case ChatNode::State::Running:
    return QObject::tr("running");
  case ChatNode::State::Done:
    return QObject::tr("done");
  case ChatNode::State::Failed:
    return QObject::tr("failed");
  case ChatNode::State::Cancelled:
    return QObject::tr("cancelled");
  case ChatNode::State::Waiting:
    return QObject::tr("waiting");
  default:
    return QString();
  }
}

bool isMono(ChatNode::Kind kind) {
  return kind == ChatNode::Kind::JobSearch ||
         kind == ChatNode::Kind::JobDelegate ||
         kind == ChatNode::Kind::JobPromote ||
         kind == ChatNode::Kind::JobRead ||
         kind == ChatNode::Kind::Tool;
}

} // namespace

ChatNodeWidget::ChatNodeWidget(const ChatNode &node, ChatTree *tree,
                               QWidget *parent)
    : QWidget(parent), m_id(node.id), m_node(node), m_tree(tree) {
  setObjectName(QStringLiteral("chatNode"));
  setAttribute(Qt::WA_StyledBackground, true);

  m_rootLayout = new QVBoxLayout(this);
  m_rootLayout->setContentsMargins(0, 0, 0, 0);
  m_rootLayout->setSpacing(4);

  applyStructure();
  applyText();
  applyState();
}

void ChatNodeWidget::updateNode(const ChatNode &node) {
  const bool textChanged = m_node.text != node.text;
  const bool stateChanged = m_node.state != node.state;
  const bool resultChanged = m_node.result != node.result;
  const bool errorChanged = m_node.error != node.error;
  const bool detailChanged = m_node.detail != node.detail;
  const bool childrenChanged = m_node.children != node.children;

  m_node = node;

  if (textChanged || detailChanged) {
    applyText();
  }

  if (stateChanged || resultChanged || errorChanged) {
    applyState();
  }

  if (childrenChanged) {
    rebuildCarousel();
  }

  if (m_tree) {
    const QStringList ids = m_openChildren.keys();

    for (const QString &childId : ids) {
      ChatNodeWidget *child = m_openChildren.value(childId, nullptr);

      if (!child) {
        continue;
      }

      const ChatNode *childNode = m_tree->node(childId);

      if (childNode) {
        child->updateNode(*childNode);
      }
    }
  }
}

void ChatNodeWidget::applyStructure() {
  if (m_structureBuilt) {
    return;
  }

  m_structureBuilt = true;

  auto *outerRow = new QHBoxLayout;
  outerRow->setContentsMargins(8, 6, 8, 6);
  outerRow->setSpacing(6);

  auto *leftColumn = new QWidget(this);
  auto *leftLayout = new QVBoxLayout(leftColumn);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(4);

  leftColumn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  auto *headerRow = new QWidget(leftColumn);
  auto *headerLayout = new QHBoxLayout(headerRow);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(6);

  if (isMono(m_node.kind)) {
    m_glyph = new QLabel(glyphFor(m_node.kind), headerRow);
    m_glyph->setObjectName(QStringLiteral("chatNodeGlyph"));
    m_glyph->setFixedWidth(16);
    headerLayout->addWidget(m_glyph);
  }

  m_title = new QLabel(headerRow);
  m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);

  QFont titleFont;

  if (isMono(m_node.kind)) {
    titleFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    titleFont.setPointSizeF(9.5);
    m_title->setObjectName(QStringLiteral("chatNodeTitle"));
  } else if (m_node.kind == ChatNode::Kind::Status) {
    titleFont.setPointSizeF(9.0);
    m_title->setObjectName(QStringLiteral("chatNodeStatus"));
  } else if (m_node.kind == ChatNode::Kind::Error) {
    titleFont.setPointSizeF(10.0);
    m_title->setObjectName(QStringLiteral("chatNodeError"));
  } else {
    titleFont.setPointSizeF(11.0);
    m_title->setObjectName(QStringLiteral("chatNodeText"));
  }

  m_title->setFont(titleFont);
  m_title->setWordWrap(!isMono(m_node.kind));

  headerLayout->addWidget(m_title, 1);

  if (isMono(m_node.kind)) {
    m_abort = new QPushButton(QStringLiteral("✕"), headerRow);
    m_abort->setObjectName(QStringLiteral("chatNodeAbort"));
    m_abort->setFlat(true);
    m_abort->setFixedSize(20, 20);
    m_abort->setCursor(Qt::PointingHandCursor);
    m_abort->setToolTip(tr("Cancel this job."));

    connect(m_abort, &QPushButton::clicked, this, [this]() {
      emit abortRequested(m_node.jobId);
    });

    headerLayout->addWidget(m_abort);
  }

  leftLayout->addWidget(headerRow);

  m_detail = new QLabel(leftColumn);
  m_detail->setObjectName(QStringLiteral("chatNodeDetail"));
  m_detail->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_detail->setWordWrap(true);
  m_detail->setVisible(false);
  leftLayout->addWidget(m_detail);

  m_bodyHost = new QWidget(leftColumn);
  m_bodyHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  m_bodyLayout = new QVBoxLayout(m_bodyHost);
  m_bodyLayout->setContentsMargins(0, 0, 0, 0);
  m_bodyLayout->setSpacing(4);
  leftLayout->addWidget(m_bodyHost);


  m_markdownBody = new MarkdownView(m_bodyHost);
  m_markdownBody->setObjectName(QStringLiteral("chatNodeMarkdown"));
  m_markdownBody->setVisible(false);
  m_bodyLayout->addWidget(m_markdownBody);

  m_errorBody = new QLabel(m_bodyHost);
  m_errorBody->setObjectName(QStringLiteral("chatNodeErrorBody"));
  m_errorBody->setWordWrap(true);
  m_errorBody->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_errorBody->setVisible(false);
  m_bodyLayout->addWidget(m_errorBody);

  m_carouselHost = new QWidget(leftColumn);
  m_carouselHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  m_carouselLayout = new QVBoxLayout(m_carouselHost);
  m_carouselLayout->setContentsMargins(16, 4, 0, 0);
  m_carouselLayout->setSpacing(4);
  leftLayout->addWidget(m_carouselHost);



  outerRow->addWidget(leftColumn, 1);

  m_arrowColumn = new QWidget(this);
  m_arrowLayout = new QVBoxLayout(m_arrowColumn);
  m_arrowLayout->setContentsMargins(0, 0, 0, 0);
  m_arrowLayout->setSpacing(2);
  m_arrowLayout->setAlignment(Qt::AlignTop);

  outerRow->addWidget(m_arrowColumn);

  m_rootLayout->addLayout(outerRow);

  rebuildCarousel();
}

void ChatNodeWidget::rebuildCarousel() {
  if (!m_arrowLayout || !m_carouselLayout) {
    return;
  }

  while (QLayoutItem *item = m_arrowLayout->takeAt(0)) {
    if (QWidget *widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }

  for (int i = 0; i < m_node.children.size(); ++i) {
    const QString childId = m_node.children.at(i);

    const bool open = m_openChildren.contains(childId);

    auto *button = new QToolButton(m_arrowColumn);
    button->setObjectName(QStringLiteral("chatNodeArrow"));
    button->setAutoRaise(true);
    button->setFixedSize(20, 20);
    button->setCursor(Qt::PointingHandCursor);
    button->setText(open ? QStringLiteral("‹") : QStringLiteral("›"));
    button->setToolTip(open ? tr("Close") : tr("Open"));

    connect(button, &QToolButton::clicked, this, [this, i, childId]() {
      if (m_openChildren.contains(childId)) {
        closeChild(childId);
      } else {
        openChild(i);
      }
    });

    m_arrowLayout->addWidget(button);
  }

  m_arrowLayout->addStretch();
}

void ChatNodeWidget::openChild(int index) {
  if (!m_tree || !m_carouselLayout) {
    return;
  }

  if (index < 0 || index >= m_node.children.size()) {
    return;
  }

  const QString childId = m_node.children.at(index);

  if (m_openChildren.contains(childId)) {
    return;
  }

  const ChatNode *child = m_tree->node(childId);

  if (!child) {
    return;
  }

  ChatNodeWidget *widget = buildChildWidget(*child);

  m_carouselLayout->addWidget(widget);
  m_openChildren.insert(childId, widget);

  rebuildCarousel();
}

void ChatNodeWidget::closeChild(const QString &childId) {
  ChatNodeWidget *widget = m_openChildren.take(childId);

  if (!widget) {
    return;
  }

  if (m_carouselLayout) {
    m_carouselLayout->removeWidget(widget);
  }

  widget->hide();
  widget->deleteLater();

  rebuildCarousel();
}

ChatNodeWidget *ChatNodeWidget::buildChildWidget(const ChatNode &child) {
  auto *widget = new ChatNodeWidget(child, m_tree, m_carouselHost);

  connect(widget, &ChatNodeWidget::abortRequested, this,
          &ChatNodeWidget::abortRequested);

  return widget;
}

void ChatNodeWidget::applyText() {
  if (isMono(m_node.kind)) {
    QString title = m_node.text;

    if (m_node.state != ChatNode::State::None) {
      title += QStringLiteral("  [");
      title += stateLabel(m_node.state);
      title += QStringLiteral("]");
    }

    m_title->setText(title);
    m_title->setVisible(true);
    m_markdownBody->setVisible(false);

    return;
  }

  if (m_node.isText()) {
    m_title->setVisible(false);

    m_markdownBody->setMarkdownText(m_node.text);
    m_markdownBody->setVisible(!m_node.text.isEmpty());

    return;
  }

  m_title->setText(m_node.text);
  m_title->setVisible(true);
  m_markdownBody->setVisible(false);
}
void ChatNodeWidget::applyState() {
  const bool jobOrTool = isMono(m_node.kind);
  const bool pending = m_node.state == ChatNode::State::Pending ||
                       m_node.state == ChatNode::State::Running ||
                       m_node.state == ChatNode::State::Waiting;

  if (m_abort) {
    m_abort->setVisible(jobOrTool && pending);
  }

  if (m_detail) {
    m_detail->setText(m_node.detail);
    m_detail->setVisible(!m_node.detail.isEmpty());
  }

  if (jobOrTool) {
    if (!m_node.result.isEmpty()) {
      m_markdownBody->setMarkdownText(m_node.result);
      m_markdownBody->setVisible(true);
    } else {
      m_markdownBody->setVisible(false);
    }

    if (!m_node.error.isEmpty()) {
      m_errorBody->setText(m_node.error);
      m_errorBody->setVisible(true);
    } else {
      m_errorBody->setVisible(false);
    }

    return;
  }

  if (m_node.kind == ChatNode::Kind::AssistantText) {
    applyText();
  }
}