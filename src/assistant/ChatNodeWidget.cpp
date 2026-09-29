#include "../../include/assistant/ChatNodeWidget.h"

#include "../../include/assistant/ChatTree.h"
#include "../../include/overseer/MarkdownView.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
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
  case ChatNode::Kind::JobEdit:
    return QStringLiteral("✎");
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
         kind == ChatNode::Kind::JobEdit ||
         kind == ChatNode::Kind::Tool;
}

} // namespace

ChatNodeWidget::ChatNodeWidget(const ChatNode &node, ChatTree *tree,
                               QWidget *parent)
    : QWidget(parent), m_id(node.id), m_node(node), m_tree(tree),
      m_lastKnownChildCount(node.children.size()) {
  setObjectName(QStringLiteral("chatNode"));
  setAttribute(Qt::WA_StyledBackground, true);

  setFocusPolicy(Qt::NoFocus);

  if (m_node.kind == ChatNode::Kind::UserText) {
    setProperty("role", QStringLiteral("user"));
  } else if (m_node.kind == ChatNode::Kind::AssistantText) {
    setProperty("role", QStringLiteral("assistant"));
  } else if (m_node.kind == ChatNode::Kind::Status) {
    setProperty("role", QStringLiteral("status"));
  } else if (m_node.kind == ChatNode::Kind::Error) {
    setProperty("role", QStringLiteral("error"));
  } else {
    setProperty("role", QStringLiteral("job"));
  }

  m_rootLayout = new QVBoxLayout(this);
  m_rootLayout->setContentsMargins(0, 0, 0, 0);
  m_rootLayout->setSpacing(6);

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

  const int previousCount = m_lastKnownChildCount;

  m_node = node;

  if (textChanged || detailChanged) {
    applyText();
  }

  if (stateChanged || resultChanged || errorChanged) {
    applyState();
  }

  const int newCount = m_node.children.size();

  if (newCount != previousCount) {
    m_lastKnownChildCount = newCount;
    rebuildCarousel();

    if (newCount > previousCount && m_followTail) {
      goToPage(newCount - 1);
    }
  }

  if (m_tree) {
    for (ChatNodeWidget *child : std::as_const(m_pagesByChildId)) {
      if (!child) {
        continue;
      }

      const ChatNode *childNode = m_tree->node(child->nodeId());

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
  outerRow->setContentsMargins(0, 0, 0, 0);
  outerRow->setSpacing(0);

  auto *leftColumn = new QWidget(this);
  auto *leftLayout = new QVBoxLayout(leftColumn);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(6);

  leftColumn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  auto *headerRow = new QWidget(leftColumn);
  auto *headerLayout = new QHBoxLayout(headerRow);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(8);

  if (isMono(m_node.kind)) {
    m_glyph = new QLabel(glyphFor(m_node.kind), headerRow);
    m_glyph->setObjectName(QStringLiteral("chatNodeGlyph"));
    m_glyph->setFixedWidth(20);
    m_glyph->setAlignment(Qt::AlignCenter);
    m_glyph->setFocusPolicy(Qt::NoFocus);
    headerLayout->addWidget(m_glyph);
  }

  m_title = new QLabel(headerRow);
  m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_title->setFocusPolicy(Qt::NoFocus);

  QFont titleFont;

  if (isMono(m_node.kind)) {
    titleFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    titleFont.setPointSizeF(10.0);
    m_title->setObjectName(QStringLiteral("chatNodeTitle"));
  } else if (m_node.kind == ChatNode::Kind::Status) {
    titleFont.setPointSizeF(9.0);
    m_title->setObjectName(QStringLiteral("chatNodeStatus"));
  } else if (m_node.kind == ChatNode::Kind::Error) {
    titleFont.setPointSizeF(10.5);
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
    m_abort->setFixedSize(22, 22);
    m_abort->setCursor(Qt::PointingHandCursor);
    m_abort->setFocusPolicy(Qt::NoFocus);
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
  m_detail->setFocusPolicy(Qt::NoFocus);
  m_detail->setVisible(false);
  leftLayout->addWidget(m_detail);

  m_bodyHost = new QWidget(leftColumn);
  m_bodyHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  m_bodyHost->setFocusPolicy(Qt::NoFocus);
  m_bodyLayout = new QVBoxLayout(m_bodyHost);
  m_bodyLayout->setContentsMargins(0, 0, 0, 0);
  m_bodyLayout->setSpacing(4);
  leftLayout->addWidget(m_bodyHost);

  m_markdownBody = new MarkdownView(m_bodyHost);
  m_markdownBody->setObjectName(QStringLiteral("chatNodeMarkdown"));
  m_markdownBody->setFocusPolicy(Qt::NoFocus);
  m_markdownBody->setVisible(false);
  m_bodyLayout->addWidget(m_markdownBody);

  m_errorBody = new QLabel(m_bodyHost);
  m_errorBody->setObjectName(QStringLiteral("chatNodeErrorBody"));
  m_errorBody->setWordWrap(true);
  m_errorBody->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_errorBody->setFocusPolicy(Qt::NoFocus);
  m_errorBody->setVisible(false);
  m_bodyLayout->addWidget(m_errorBody);

  m_carouselHost = new QWidget(leftColumn);
  m_carouselHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  m_carouselHost->setObjectName(QStringLiteral("chatNodeCarouselHost"));
  m_carouselHost->setFocusPolicy(Qt::NoFocus);

  m_carouselLayout = new QVBoxLayout(m_carouselHost);
  m_carouselLayout->setContentsMargins(0, 4, 0, 0);
  m_carouselLayout->setSpacing(6);

  m_pages = new QStackedWidget(m_carouselHost);
  m_pages->setObjectName(QStringLiteral("chatNodePages"));
  m_pages->setFocusPolicy(Qt::NoFocus);
  m_carouselLayout->addWidget(m_pages);

  m_chromeRow = new QWidget(m_carouselHost);
  m_chromeRow->setObjectName(QStringLiteral("chatNodeChrome"));
  m_chromeRow->setFocusPolicy(Qt::NoFocus);
  m_chromeRow->setVisible(false);

  m_chromeLayout = new QHBoxLayout(m_chromeRow);
  m_chromeLayout->setContentsMargins(0, 0, 0, 0);
  m_chromeLayout->setSpacing(6);

  m_prevButton = new QToolButton(m_chromeRow);
  m_prevButton->setObjectName(QStringLiteral("chatNodePrev"));
  m_prevButton->setText(QStringLiteral("←"));
  m_prevButton->setAutoRaise(true);
  m_prevButton->setFocusPolicy(Qt::NoFocus);
  m_prevButton->setCursor(Qt::PointingHandCursor);
  m_prevButton->setFixedSize(22, 22);
  m_prevButton->setToolTip(tr("Previous step"));

  m_nextButton = new QToolButton(m_chromeRow);
  m_nextButton->setObjectName(QStringLiteral("chatNodeNext"));
  m_nextButton->setText(QStringLiteral("→"));
  m_nextButton->setAutoRaise(true);
  m_nextButton->setFocusPolicy(Qt::NoFocus);
  m_nextButton->setCursor(Qt::PointingHandCursor);
  m_nextButton->setFixedSize(22, 22);
  m_nextButton->setToolTip(tr("Next step"));

  m_dotsHost = new QWidget(m_chromeRow);
  m_dotsHost->setObjectName(QStringLiteral("chatNodeDots"));
  m_dotsHost->setFocusPolicy(Qt::NoFocus);
  m_dotsLayout = new QHBoxLayout(m_dotsHost);
  m_dotsLayout->setContentsMargins(0, 0, 0, 0);
  m_dotsLayout->setSpacing(4);

  m_counter = new QLabel(m_chromeRow);
  m_counter->setObjectName(QStringLiteral("chatNodeCounter"));
  m_counter->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_counter->setFocusPolicy(Qt::NoFocus);

  m_chromeLayout->addWidget(m_prevButton);
  m_chromeLayout->addWidget(m_dotsHost);
  m_chromeLayout->addWidget(m_nextButton);
  m_chromeLayout->addStretch(1);
  m_chromeLayout->addWidget(m_counter);

  m_carouselLayout->addWidget(m_chromeRow);

  leftLayout->addWidget(m_carouselHost);

  outerRow->addWidget(leftColumn, 1);

  m_rootLayout->addLayout(outerRow);

  connect(m_prevButton, &QToolButton::clicked, this,
          &ChatNodeWidget::goPrev);
  connect(m_nextButton, &QToolButton::clicked, this,
          &ChatNodeWidget::goNext);

  rebuildCarousel();
}

void ChatNodeWidget::rebuildCarousel() {
  if (!m_pages || !m_chromeRow || !m_dotsLayout) {
    return;
  }

  buildPages();
  rebuildDots();

  const int count = m_pages->count();

  if (count == 0) {
    m_chromeRow->setVisible(false);
    m_currentPage = -1;
    updatePageChrome();
    return;
  }

  m_chromeRow->setVisible(true);

  if (m_currentPage < 0 || m_currentPage >= count) {
    m_currentPage = count - 1;
  }

  m_pages->setCurrentIndex(m_currentPage);

  updatePageChrome();
}

void ChatNodeWidget::buildPages() {
  if (!m_pages) {
    return;
  }

  QSet<QString> wanted;

  for (const QString &childId : m_node.children) {
    wanted.insert(childId);
  }

  const QStringList existing = m_pagesByChildId.keys();

  for (const QString &childId : existing) {
    if (wanted.contains(childId)) {
      continue;
    }

    ChatNodeWidget *page = m_pagesByChildId.take(childId);

    if (page) {
      m_pages->removeWidget(page);
      page->hide();
      page->deleteLater();
    }
  }

  for (const QString &childId : m_node.children) {
    ensurePage(childId);
  }
}

void ChatNodeWidget::ensurePage(const QString &childId) {
  if (!m_pages || !m_tree) {
    return;
  }

  if (m_pagesByChildId.contains(childId)) {
    return;
  }

  const ChatNode *child = m_tree->node(childId);

  if (!child) {
    return;
  }

  ChatNodeWidget *page = buildChildWidget(*child);

  m_pages->addWidget(page);
  m_pagesByChildId.insert(childId, page);
}

void ChatNodeWidget::rebuildDots() {
  if (!m_dotsLayout) {
    return;
  }

  while (QLayoutItem *item = m_dotsLayout->takeAt(0)) {
    if (QWidget *widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }

  m_dotsByChildId.clear();

  for (int i = 0; i < m_node.children.size(); ++i) {
    const QString childId = m_node.children.at(i);

    auto *dot = new QToolButton(m_dotsHost);
    dot->setObjectName(QStringLiteral("chatNodeDot"));
    dot->setAutoRaise(true);
    dot->setFocusPolicy(Qt::NoFocus);
    dot->setCursor(Qt::PointingHandCursor);
    dot->setFixedSize(10, 10);
    dot->setText(QString());
    dot->setToolTip(tr("Step %1").arg(i + 1));

    connect(dot, &QToolButton::clicked, this,
            [this, i]() { onDotClicked(i); });

    m_dotsLayout->addWidget(dot);
    m_dotsByChildId.insert(childId, dot);
  }
}

void ChatNodeWidget::updatePageChrome() {
  const int count = m_pages ? m_pages->count() : 0;

  if (m_prevButton) {
    m_prevButton->setEnabled(count > 0 && m_currentPage > 0);
  }

  if (m_nextButton) {
    m_nextButton->setEnabled(count > 0 && m_currentPage < count - 1);
  }

  if (m_counter) {
    if (count <= 0) {
      m_counter->setText(QString());
    } else {
      m_counter->setText(QStringLiteral("%1 / %2")
                             .arg(m_currentPage + 1)
                             .arg(count));
    }
  }

  for (int i = 0; i < m_node.children.size(); ++i) {
    const QString childId = m_node.children.at(i);

    QToolButton *dot = m_dotsByChildId.value(childId, nullptr);

    if (!dot) {
      continue;
    }

    const bool current = (i == m_currentPage);

    dot->setProperty("current", current);
    dot->style()->unpolish(dot);
    dot->style()->polish(dot);
  }
}

void ChatNodeWidget::goToPage(int index) {
  if (!m_pages) {
    return;
  }

  const int count = m_pages->count();

  if (count == 0) {
    return;
  }

  const int clamped = qBound(0, index, count - 1);

  if (m_currentPage == clamped) {
    updatePageChrome();
    return;
  }

  m_currentPage = clamped;
  m_pages->setCurrentIndex(clamped);

  updatePageChrome();
}

void ChatNodeWidget::goPrev() {
  if (m_currentPage <= 0) {
    return;
  }

  m_followTail = false;
  goToPage(m_currentPage - 1);
}

void ChatNodeWidget::goNext() {
  if (!m_pages) {
    return;
  }

  const int count = m_pages->count();

  if (m_currentPage >= count - 1) {
    return;
  }

  const int target = m_currentPage + 1;

  if (target == count - 1) {
    m_followTail = true;
  }

  goToPage(target);
}

void ChatNodeWidget::onDotClicked(int index) {
  if (!m_pages) {
    return;
  }

  const int count = m_pages->count();

  if (index == count - 1) {
    m_followTail = true;
  } else {
    m_followTail = false;
  }

  goToPage(index);
}

ChatNodeWidget *ChatNodeWidget::buildChildWidget(const ChatNode &child) {
  auto *widget = new ChatNodeWidget(child, m_tree, m_pages);

  connect(widget, &ChatNodeWidget::abortRequested, this,
          &ChatNodeWidget::abortRequested);

  return widget;
}

void ChatNodeWidget::applyText() {
  if (isMono(m_node.kind)) {
    QString title = m_node.text;

    if (m_node.state != ChatNode::State::None) {
      title += QStringLiteral("  ·  ");
      title += stateLabel(m_node.state);
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