#include "../../include/assistant/AssistantShell.h"

#include "../../include/app/theme/ThemeRegistry.h"
#include "../../include/assistant/ChatNodeWidget.h"
#include "../../include/assistant/MindMapScene.h"
#include "../../include/assistant/MindMapView.h"
#include "../../include/avatar/AvatarWidget.h"
#include "../../include/overseer/OverseerSessionManager.h"
#include "../../include/voice/SpeechController.h"

#include <QComboBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShowEvent>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr auto kPolicyKey = "assistant/completionPolicy";

constexpr int kShellMargin = 32;
constexpr int kHeaderSpacing = 16;

bool isTopLevelNode(ChatNode::Kind kind) {
  return kind == ChatNode::Kind::UserText ||
         kind == ChatNode::Kind::AssistantText;
}

} // namespace

AssistantShell::AssistantShell(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("assistantShell"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFocusPolicy(Qt::StrongFocus);

  m_tokens = ThemeRegistry::instance().tokens(
      ThemeRegistry::instance().activeTheme());

  m_tree = new ChatTree(this);

  connect(m_tree, &ChatTree::nodeAdded, this,
          &AssistantShell::onNodeAdded);
  connect(m_tree, &ChatTree::nodeChanged, this,
          &AssistantShell::onNodeChanged);
  connect(m_tree, &ChatTree::cleared, this,
          &AssistantShell::onTreeCleared);

  buildUi();
}

AssistantShell::~AssistantShell() = default;

void AssistantShell::setThemeTokens(const ThemeTokens &tokens) {
  m_tokens = tokens;
  update();
}

void AssistantShell::setAssistant(LoreAssistant *assistant) {
  if (m_assistant) {
    disconnect(m_assistant, nullptr, this, nullptr);
  }

  m_assistant = assistant;

  if (!m_assistant) {
    return;
  }

  QSettings settings;
  const int stored =
      settings.value(kPolicyKey,
                     static_cast<int>(LoreAssistant::CompletionPolicy::Automatic))
          .toInt();

  if (m_policy) {
    const int index = m_policy->findData(stored);
    if (index >= 0) {
      m_policy->blockSignals(true);
      m_policy->setCurrentIndex(index);
      m_policy->blockSignals(false);
    }
  }

  m_assistant->setCompletionPolicy(
      static_cast<LoreAssistant::CompletionPolicy>(stored));

  connect(m_assistant, &LoreAssistant::assistantReplyStarted, this,
          &AssistantShell::onAssistantReplyStarted);
  connect(m_assistant, &LoreAssistant::assistantChunk, this,
          &AssistantShell::onAssistantChunk);
  connect(m_assistant, &LoreAssistant::assistantTurnFinished, this,
          &AssistantShell::onAssistantTurnFinished);

  connect(m_assistant, &LoreAssistant::jobCreated, this,
          &AssistantShell::onJobCreated);
  connect(m_assistant, &LoreAssistant::jobCompleted, this,
          &AssistantShell::onJobCompleted);
  connect(m_assistant, &LoreAssistant::jobFailed, this,
          &AssistantShell::onJobFailed);

  connect(m_assistant, &LoreAssistant::statusMessage, this,
          &AssistantShell::onStatusMessage);
  connect(m_assistant, &LoreAssistant::statusChanged, this,
          &AssistantShell::onStatusChanged);
}

void AssistantShell::setSpeechController(SpeechController *speech) {
  if (m_speech) {
    disconnect(m_speech, nullptr, this, nullptr);
  }

  m_speech = speech;

  if (!m_speech) {
    return;
  }

  connect(m_speech, &SpeechController::transcribed, this,
          &AssistantShell::onTranscribed);
  connect(m_speech, &SpeechController::liveTranscribed, this,
          &AssistantShell::onLiveTranscribed);
  connect(m_speech, &SpeechController::stateChanged, this,
          &AssistantShell::onSpeechStateChanged);

  onSpeechStateChanged();
}

void AssistantShell::setOverseerManager(OverseerSessionManager *manager) {
  if (m_overseer) {
    disconnect(m_overseer, nullptr, this, nullptr);
  }

  m_overseer = manager;

  if (m_mindScene) {
    m_mindScene->setOverseerManager(manager);
  }

  if (m_overseer) {
    connect(m_overseer, &OverseerSessionManager::sessionListChanged,
            this, &AssistantShell::onOverseerSessionListChanged);
  }
}

void AssistantShell::setAvatar(AvatarWidget *avatar) {
  if (m_avatar == avatar) {
    return;
  }

  m_avatar = avatar;

  if (!m_avatar) {
    return;
  }

  m_avatar->setParent(this);
  m_avatar->setAttribute(Qt::WA_TranslucentBackground, true);
  m_avatar->setResizable(true);

  const int side = m_avatar->defaultSize().width();
  m_avatar->resize(side, side);
  m_avatar->show();
  m_avatar->raise();

  m_avatarPlaced = false;

  placeAvatarOnce();
}

void AssistantShell::buildUi() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(kShellMargin, 24, kShellMargin, kShellMargin);
  root->setSpacing(kHeaderSpacing);

  root->addWidget(buildHeader(), 0);

  m_tabs = new QTabWidget(this);
  m_tabs->setObjectName(QStringLiteral("assistantShellTabs"));
  m_tabs->setDocumentMode(true);
  m_tabs->addTab(buildChatTab(), tr("Chat"));
  m_tabs->addTab(buildMindTab(), tr("Mind"));

  root->addWidget(m_tabs, 1);
  root->addWidget(buildControls(), 0);

  connect(m_tabs, &QTabWidget::currentChanged, this,
          &AssistantShell::onTabChanged);
}

QWidget *AssistantShell::buildHeader() {
  m_header = new QWidget(this);
  m_header->setObjectName(QStringLiteral("assistantShellHeader"));

  m_title = new QLabel(tr("Lore"), m_header);
  m_title->setObjectName(QStringLiteral("assistantShellTitle"));
  m_title->setFocusPolicy(Qt::NoFocus);

  {
    QFont f = m_title->font();
    f.setPointSizeF(f.pointSizeF() * 1.5);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 4.0);
    f.setWeight(QFont::Light);
    m_title->setFont(f);
  }

  m_status = new QLabel(tr("Idle"), m_header);
  m_status->setObjectName(QStringLiteral("assistantShellStatus"));
  m_status->setFocusPolicy(Qt::NoFocus);

  m_policy = new QComboBox(m_header);
  m_policy->setObjectName(QStringLiteral("assistantShellPolicy"));
  m_policy->setFocusPolicy(Qt::NoFocus);
  m_policy->addItem(tr("Results: automatic"),
                    static_cast<int>(
                        LoreAssistant::CompletionPolicy::Automatic));
  m_policy->addItem(tr("Results: paste in chat"),
                    static_cast<int>(
                        LoreAssistant::CompletionPolicy::PasteInChat));
  m_policy->addItem(tr("Results: feed to Lore"),
                    static_cast<int>(
                        LoreAssistant::CompletionPolicy::FeedToQueue));
  m_policy->addItem(tr("Results: next message"),
                    static_cast<int>(
                        LoreAssistant::CompletionPolicy::AppendToNextUserMessage));

  auto *layout = new QHBoxLayout(m_header);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(16);
  layout->addWidget(m_title);
  layout->addWidget(m_status);
  layout->addStretch(1);
  layout->addWidget(m_policy);

  connect(m_policy, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &AssistantShell::onPolicyChanged);

  return m_header;
}

QWidget *AssistantShell::buildChatTab() {
  m_chatPage = new QWidget(m_tabs);

  m_chatScroll = new QScrollArea(m_chatPage);
  m_chatScroll->setObjectName(QStringLiteral("assistantShellChatScroll"));
  m_chatScroll->setWidgetResizable(true);
  m_chatScroll->setFrameShape(QFrame::NoFrame);
  m_chatScroll->setFocusPolicy(Qt::NoFocus);

  m_chatHost = new QWidget;
  m_chatHost->setFocusPolicy(Qt::NoFocus);

  m_chatLayout = new QVBoxLayout(m_chatHost);
  m_chatLayout->setContentsMargins(0, 16, 0, 16);
  m_chatLayout->setSpacing(0);
  m_chatLayout->setAlignment(Qt::AlignTop);

  m_hero = buildHero();
  m_chatLayout->addWidget(m_hero);

  m_chatLayout->addStretch(1);

  m_chatScroll->setWidget(m_chatHost);

  auto *layout = new QVBoxLayout(m_chatPage);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_chatScroll, 1);

  return m_chatPage;
}

QWidget *AssistantShell::buildHero() {
  auto *hero = new QWidget;
  hero->setObjectName(QStringLiteral("assistantShellHero"));
  hero->setFocusPolicy(Qt::NoFocus);

  auto *title = new QLabel(tr("Ask me anything."), hero);
  title->setObjectName(QStringLiteral("assistantShellHeroTitle"));
  title->setAlignment(Qt::AlignCenter);
  title->setFocusPolicy(Qt::NoFocus);

  {
    QFont f = title->font();
    f.setPointSizeF(f.pointSizeF() * 1.3);
    f.setWeight(QFont::Light);
    title->setFont(f);
  }

  auto *hint = new QLabel(
      tr("I read your notes, remember what matters, and put the "
         "Overseer to work."),
      hero);
  hint->setObjectName(QStringLiteral("assistantShellHeroHint"));
  hint->setAlignment(Qt::AlignCenter);
  hint->setWordWrap(true);
  hint->setFocusPolicy(Qt::NoFocus);

  auto *layout = new QVBoxLayout(hero);
  layout->setContentsMargins(48, 64, 48, 64);
  layout->setSpacing(12);
  layout->addStretch(1);
  layout->addWidget(title);
  layout->addWidget(hint);
  layout->addStretch(1);

  return hero;
}

QWidget *AssistantShell::buildMindTab() {
  auto *page = new QWidget(m_tabs);

  m_mindScene = new MindMapScene(this);
  m_mindScene->setOverseerManager(m_overseer);

  m_mindView = new MindMapView(m_mindScene, page);
  m_mindView->setObjectName(QStringLiteral("assistantShellMindView"));
  m_mindView->setFocusPolicy(Qt::NoFocus);

  connect(m_mindView, &MindMapView::openRequested, this,
          [this](const QString &path) {
            Q_UNUSED(path);
          });

  connect(m_mindView, &MindMapView::sessionOpenRequested, this,
          [this](const QString &name) {
            if (m_overseer) {
              m_overseer->openSession(name);
            }
          });

  connect(m_mindView, &MindMapView::refreshRequested, this,
          &AssistantShell::refreshMindMap);

  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_mindView, 1);

  return page;
}

QWidget *AssistantShell::buildControls() {
  m_controls = new QWidget(this);
  m_controls->setObjectName(QStringLiteral("assistantShellComposer"));
  m_controls->setFocusPolicy(Qt::NoFocus);

  auto makeSpeechButton = [this](const QString &name,
                                  const QString &glyph,
                                  const QString &tooltip) {
    auto *button = new QToolButton(m_controls);
    button->setObjectName(name);
    button->setText(glyph);
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(38, 38);
    return button;
  };

  m_dictate = makeSpeechButton(QStringLiteral("assistantShellDictate"),
                               QStringLiteral("●"),
                               tr("Dictate: record once, transcribe, send."));
  m_live = makeSpeechButton(QStringLiteral("assistantShellLive"),
                            QStringLiteral("◉"),
                            tr("Live dictate: stream speech to text."));
  m_readAloud = makeSpeechButton(
      QStringLiteral("assistantShellReadAloud"), QStringLiteral("▶"),
      tr("Read the last reply aloud."));

  m_input = new QLineEdit(m_controls);
  m_input->setObjectName(QStringLiteral("assistantShellInput"));
  m_input->setPlaceholderText(tr("Message Lore"));
  m_input->setClearButtonEnabled(true);
  m_input->setMinimumHeight(42);
  m_input->setFocusPolicy(Qt::StrongFocus);

  m_abort = new QPushButton(QStringLiteral("✕"), m_controls);
  m_abort->setObjectName(QStringLiteral("assistantShellAbort"));
  m_abort->setCursor(Qt::PointingHandCursor);
  m_abort->setFixedSize(38, 38);
  m_abort->setFocusPolicy(Qt::NoFocus);
  m_abort->setToolTip(tr("Cancel all in-flight work."));
  m_abort->setVisible(false);

  m_send = new QPushButton(tr("Send"), m_controls);
  m_send->setObjectName(QStringLiteral("assistantShellSend"));
  m_send->setCursor(Qt::PointingHandCursor);
  m_send->setMinimumHeight(42);
  m_send->setMinimumWidth(88);
  m_send->setFocusPolicy(Qt::NoFocus);

  auto *layout = new QHBoxLayout(m_controls);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);
  layout->addWidget(m_dictate);
  layout->addWidget(m_live);
  layout->addWidget(m_readAloud);
  layout->addSpacing(4);
  layout->addWidget(m_input, 1);
  layout->addWidget(m_abort);
  layout->addWidget(m_send);

  connect(m_send, &QPushButton::clicked, this, &AssistantShell::onSubmit);
  connect(m_input, &QLineEdit::returnPressed, this,
          &AssistantShell::onSubmit);
  connect(m_abort, &QPushButton::clicked, this,
          &AssistantShell::onAbortClicked);
  connect(m_dictate, &QToolButton::clicked, this,
          &AssistantShell::onDictateClicked);
  connect(m_live, &QToolButton::clicked, this,
          &AssistantShell::onLiveDictateClicked);
  connect(m_readAloud, &QToolButton::clicked, this,
          &AssistantShell::onReadAloudClicked);

  return m_controls;
}

void AssistantShell::focusPrompt() {
  if (m_input) {
    m_input->setFocus(Qt::OtherFocusReason);
  }
}

void AssistantShell::setReplyText(const QString &text) {
  Q_UNUSED(text);
}

QString AssistantShell::beginUserMessage(const QString &text) {
  return m_tree->appendText(ChatNode::Kind::UserText, text, QString());
}

void AssistantShell::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  placeAvatarOnce();
  focusPrompt();
}

void AssistantShell::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
}

void AssistantShell::placeAvatarOnce() {
  if (!m_avatar || m_avatarPlaced) {
    if (m_avatar) {
      m_avatar->raise();
    }
    return;
  }

  QTimer::singleShot(0, this, [this]() {
    if (!m_avatar || m_avatarPlaced) {
      return;
    }

    const int x = (width() - m_avatar->width()) / 2;
    const int y = (height() - m_avatar->height()) / 2;

    m_avatar->move(x, y);
    m_avatar->raise();

    m_avatarPlaced = true;
  });
}

void AssistantShell::updateHeroVisibility() {
  if (!m_hero) {
    return;
  }

  const bool empty = m_tree && m_tree->roots().isEmpty();
  m_hero->setVisible(empty);
}

void AssistantShell::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    emit dismissed();
    event->accept();
    return;
  }

  QWidget::keyPressEvent(event);
}

void AssistantShell::onSubmit() {
  if (m_busy || !m_input) {
    return;
  }

  const QString text = m_input->text().trimmed();

  if (text.isEmpty()) {
    return;
  }

  m_input->clear();

  m_activeReplyNode.clear();

  if (m_tree) {
    m_tree->appendText(ChatNode::Kind::UserText, text, QString());
  }

  emit messageSubmitted(text);

  if (m_input) {
    m_input->setFocus(Qt::OtherFocusReason);
  }
}

void AssistantShell::onAbortClicked() {
  if (m_assistant) {
    m_assistant->abortAll();
  }
}

void AssistantShell::onDictateClicked() {
  if (!m_speech) {
    return;
  }

  if (m_speech->isCapturing()) {
    m_speech->endCapture();
    return;
  }

  m_speech->beginCapture();
}

void AssistantShell::onLiveDictateClicked() {
  if (!m_speech) {
    return;
  }

  if (m_speech->isLiveCapturing() || m_speech->isLiveStarting()) {
    m_speech->stopLiveCapture();
    return;
  }

  m_speech->startLiveCapture();
}

void AssistantShell::onReadAloudClicked() {
  if (!m_speech || !m_assistant) {
    return;
  }

  const QString last = m_assistant->lastReply();

  if (last.isEmpty()) {
    return;
  }

  m_speech->speakText(last);
}

void AssistantShell::onSpeechStateChanged() {
  if (!m_speech) {
    return;
  }

  applySpeechButtonState(m_dictate, m_speech->isCapturing());
  applySpeechButtonState(
      m_live, m_speech->isLiveCapturing() || m_speech->isLiveStarting());
  applySpeechButtonState(m_readAloud, m_speech->isSpeaking());
}

void AssistantShell::applySpeechButtonState(QToolButton *button,
                                             bool active) {
  if (!button) {
    return;
  }

  button->setProperty("active", active);
  button->style()->unpolish(button);
  button->style()->polish(button);
}

void AssistantShell::onTranscribed(const QString &text) {
  if (text.trimmed().isEmpty()) {
    return;
  }

  if (m_input) {
    m_input->setText(text);
  }

  onSubmit();
}

void AssistantShell::onLiveTranscribed(const QString &text, bool isFinal) {
  if (!m_input || text.isEmpty()) {
    return;
  }

  m_input->setText(text);
  m_input->setCursorPosition(text.length());

  if (isFinal) {
    onSubmit();
  }
}

void AssistantShell::onPolicyChanged(int index) {
  if (!m_assistant || !m_policy) {
    return;
  }

  const int value = m_policy->itemData(index).toInt();

  m_assistant->setCompletionPolicy(
      static_cast<LoreAssistant::CompletionPolicy>(value));

  QSettings settings;
  settings.setValue(kPolicyKey, value);
}

void AssistantShell::onTabChanged(int index) {
  if (index != 1) {
    return;
  }

  if (!m_mindScene || !m_mindView || !m_assistant) {
    return;
  }

  const QString root = m_assistant->rootPath();

  if (root.isEmpty()) {
    return;
  }

  if (m_mindScene->assistantRoot() != root) {
    m_mindScene->setAssistantRoot(root);
    m_mindScene->build();
    m_mindView->refresh();
  }
}

void AssistantShell::onOverseerSessionListChanged() {
  if (!m_tabs || m_tabs->currentIndex() != 1) {
    return;
  }

  refreshMindMap();
}

void AssistantShell::refreshMindMap() {
  if (!m_mindScene || !m_mindView || !m_assistant) {
    return;
  }

  const QString root = m_assistant->rootPath();

  if (root.isEmpty()) {
    return;
  }

  m_mindScene->setAssistantRoot(root);
  m_mindScene->refresh();
  m_mindView->refresh();
}

void AssistantShell::onAssistantReplyStarted(const QString &nodeId) {
  if (!m_tree || nodeId.isEmpty()) {
    return;
  }

  ChatNode node(ChatNode::Kind::AssistantText, QString());

  m_tree->appendWithId(node, nodeId, QString());

  m_activeReplyNode = nodeId;
}

void AssistantShell::onAssistantChunk(const QString &nodeId,
                                      const QString &text) {
  if (!m_tree || nodeId.isEmpty()) {
    return;
  }

  m_tree->appendText(nodeId, text);
}

void AssistantShell::onAssistantTurnFinished(const QString &nodeId) {
  Q_UNUSED(nodeId);

  setBusy(false);

  if (m_input && isVisible()) {
    m_input->setFocus(Qt::OtherFocusReason);
  }
}

void AssistantShell::onJobCreated(const QString &jobId,
                                  const QString &nodeId,
                                  ChatNode::Kind kind,
                                  const QString &title,
                                  const QString &detail) {
  if (!m_tree) {
    return;
  }

  const QString parent =
      nodeId.isEmpty() ? m_activeReplyNode : nodeId;

  m_tree->appendJob(kind, title, detail, jobId, parent);
}

void AssistantShell::onJobCompleted(const QString &jobId,
                                    const QString &result) {
  if (!m_tree) {
    return;
  }

  const QString nodeId = m_tree->nodeForJob(jobId);

  if (!nodeId.isEmpty()) {
    m_tree->setResult(nodeId, result);
  }
}

void AssistantShell::onJobFailed(const QString &jobId,
                                 const QString &error) {
  if (!m_tree) {
    return;
  }

  const QString nodeId = m_tree->nodeForJob(jobId);

  if (!nodeId.isEmpty()) {
    m_tree->setError(nodeId, error);
  }
}

void AssistantShell::onStatusMessage(const QString &text) {
  if (!m_tree) {
    return;
  }

  m_tree->appendText(ChatNode::Kind::Status, text, m_activeReplyNode);
}

void AssistantShell::onStatusChanged(const QString &status) {
  if (m_status) {
    m_status->setText(status);
  }
}

void AssistantShell::onNodeAdded(const QString &id) {
  if (!m_tree) {
    return;
  }

  const ChatNode *node = m_tree->node(id);

  if (!node) {
    return;
  }

  if (!isTopLevelNode(node->kind)) {
    updateWidget(node->parentId);
    return;
  }

  appendTopLevelWidget(id);
  updateHeroVisibility();
}

void AssistantShell::onNodeChanged(const QString &id) {
  if (!m_tree) {
    return;
  }

  const ChatNode *node = m_tree->node(id);

  if (!node) {
    return;
  }

  if (isTopLevelNode(node->kind)) {
    updateWidget(id);
  } else {
    updateWidget(node->parentId);
  }

  if (node->isJob()) {
    const int count = m_tree->inFlightCount();

    if (count > 0) {
      if (m_status) {
        m_status->setText(tr("%n job(s) in flight", "", count));
      }
      if (m_abort) {
        m_abort->setVisible(true);
      }
    } else {
      if (m_status) {
        m_status->setText(tr("Idle"));
      }
      if (m_abort) {
        m_abort->setVisible(false);
      }
    }
  }
}

void AssistantShell::appendTopLevelWidget(const QString &nodeId) {
  const ChatNode *node = m_tree->node(nodeId);

  if (!node) {
    return;
  }

  const bool isAssistant = (node->kind == ChatNode::Kind::AssistantText);

  const int insertAt = m_chatLayout->count() - 1;

  if (insertAt > 1) {
    const int gap = isAssistant ? 8 : 28;
    m_chatLayout->insertSpacing(insertAt, gap);
  }

  auto *widget = new ChatNodeWidget(*node, m_tree, m_chatHost);

  connect(widget, &ChatNodeWidget::abortRequested, this,
          [this](const QString &jobId) {
            if (!m_assistant || jobId.isEmpty()) {
              if (m_assistant) {
                m_assistant->abortAll();
              }
              return;
            }

            m_assistant->abortJob(jobId);
          });

  m_chatLayout->insertWidget(m_chatLayout->count() - 1, widget);
  m_topLevelWidgets.insert(nodeId, widget);

  scrollToBottom();
}

void AssistantShell::updateWidget(const QString &nodeId) {
  if (nodeId.isEmpty()) {
    return;
  }

  ChatNodeWidget *widget = m_topLevelWidgets.value(nodeId, nullptr);

  if (!widget || !m_tree) {
    return;
  }

  const ChatNode *node = m_tree->node(nodeId);

  if (!node) {
    return;
  }

  widget->updateNode(*node);
}

void AssistantShell::scrollToBottom() {
  if (!m_chatScroll) {
    return;
  }

  QScrollBar *bar = m_chatScroll->verticalScrollBar();

  if (bar) {
    bar->setValue(bar->maximum());
  }

  if (m_input && isVisible()) {
    m_input->setFocus(Qt::OtherFocusReason);
  }
}

void AssistantShell::onTreeCleared() {
  for (ChatNodeWidget *widget : std::as_const(m_topLevelWidgets)) {
    if (widget) {
      m_chatLayout->removeWidget(widget);
      widget->hide();
      widget->deleteLater();
    }
  }

  m_topLevelWidgets.clear();
  m_activeReplyNode.clear();

  if (m_status) {
    m_status->setText(tr("Idle"));
  }
  if (m_abort) {
    m_abort->setVisible(false);
  }

  updateHeroVisibility();
}

void AssistantShell::setBusy(bool busy) {
  m_busy = busy;

  if (m_send) {
    m_send->setEnabled(!busy);
  }

  if (m_dictate) {
    m_dictate->setEnabled(!busy);
  }

  if (m_live) {
    m_live->setEnabled(!busy);
  }
}