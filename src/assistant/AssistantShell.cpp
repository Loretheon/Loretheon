#include "../../include/assistant/AssistantShell.h"

#include "../../include/app/theme/ThemeRegistry.h"
#include "../../include/assistant/ChatNodeWidget.h"
#include "../../include/assistant/ChatTreeStore.h"
#include "../../include/assistant/MindMapScene.h"
#include "../../include/assistant/MindMapView.h"
#include "../../include/avatar/AvatarWidget.h"
#include "../../include/overseer/OverseerSessionManager.h"
#include "../../include/ui/AutoHideDock.h"
#include "../../include/ui/DockReservation.h"
#include "../../include/voice/SpeechController.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
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

constexpr auto kLastChatKey = "assistant/lastChatPath";

constexpr int kShellMargin = 32;
constexpr int kHeaderSpacing = 16;

constexpr int kExplorerEntryRole = Qt::UserRole + 1;

constexpr int kExplorerWidth = 280;

bool isTopLevelNode(ChatNode::Kind kind) {
  return kind == ChatNode::Kind::UserText ||
         kind == ChatNode::Kind::AssistantText ||
         kind == ChatNode::Kind::Status ||
         kind == ChatNode::Kind::Error;
}

bool isHeaderItem(const QListWidgetItem *item) {
  if (!item)
    return false;

  return item->data(Qt::UserRole).toString() == QStringLiteral("__header__");
}

} // namespace

AssistantShell::AssistantShell(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("assistantShell"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFocusPolicy(Qt::StrongFocus);

  m_tokens = ThemeRegistry::instance().tokens(
      ThemeRegistry::instance().activeTheme());

  m_tree = new ChatTree(this);
  m_store = new ChatTreeStore(this);

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
  connect(m_assistant, &LoreAssistant::knowledgeChanged, this,
          &AssistantShell::refreshMindMap);

  if (m_mindView) {
    m_mindView->setAssistant(m_assistant);
  }

  if (m_store) {
    m_store->setRoot(m_assistant->rootPath());

    openInitialSegment();

    reloadExplorer();
    updateHeroVisibility();
  }
}

void AssistantShell::openInitialSegment() {
  if (!m_store || !m_tree) {
    return;
  }

  QSettings settings;
  const QString remembered = settings.value(kLastChatKey).toString();

  if (!remembered.isEmpty() && QFileInfo::exists(remembered)) {
    if (m_store->openSegment(remembered)) {
      m_loading = true;
      m_store->load(m_tree);
      m_loading = false;

      if (m_assistant) {
        m_assistant->setCurrentChat(m_tree);
      }

      return;
    }
  }

  const QVector<ChatTreeStore::Entry> entries = m_store->listSegments();

  if (!entries.isEmpty()) {
    const QString newest = entries.first().absolutePath;

    if (m_store->openSegment(newest)) {
      m_loading = true;
      m_store->load(m_tree);
      m_loading = false;

      if (m_assistant) {
        m_assistant->setCurrentChat(m_tree);
      }

      rememberCurrentSegment();
      return;
    }
  }

  m_store->beginNewSegment();

  if (m_assistant) {
    m_assistant->setCurrentChat(m_tree);
  }

  rememberCurrentSegment();
}

void AssistantShell::rememberCurrentSegment() {
  if (!m_store) {
    return;
  }

  const QString path = m_store->currentPath();

  if (path.isEmpty()) {
    return;
  }

  QSettings settings;
  settings.setValue(kLastChatKey, path);
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
  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  m_explorerDock = new AutoHideDock(AutoHideDock::Edge::Left,
                                    QStringLiteral("assistant/explorer"),
                                    this);

  m_explorerDock->setContent(buildExplorer());
  m_explorerDock->setPreferredContentLength(kExplorerWidth);

  m_explorerReservation = new DockReservation(m_explorerDock, this);

  root->addWidget(m_explorerReservation, 0);

  auto *mainColumn = new QWidget(this);
  auto *column = new QVBoxLayout(mainColumn);
  column->setContentsMargins(kShellMargin, 24, kShellMargin, kShellMargin);
  column->setSpacing(kHeaderSpacing);

  column->addWidget(buildHeader(), 0);

  m_tabs = new QTabWidget(mainColumn);
  m_tabs->setObjectName(QStringLiteral("assistantShellTabs"));
  m_tabs->setDocumentMode(true);
  m_tabs->addTab(buildChatTab(), tr("Chat"));
  m_tabs->addTab(buildMindTab(), tr("Mind"));

  column->addWidget(m_tabs, 1);
  column->addWidget(buildControls(), 0);

  root->addWidget(mainColumn, 1);

  connect(m_tabs, &QTabWidget::currentChanged, this,
          &AssistantShell::onTabChanged);
}

QWidget *AssistantShell::buildExplorer() {
  auto *panel = new QWidget;

  auto *layout = new QVBoxLayout(panel);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  auto *headingRow = new QWidget(panel);
  auto *headingLayout = new QHBoxLayout(headingRow);
  headingLayout->setContentsMargins(0, 0, 0, 0);
  headingLayout->setSpacing(4);

  auto *heading = new QLabel(tr("Chats"), headingRow);
  heading->setObjectName(QStringLiteral("assistantExplorerHeading"));

  m_explorerNew = new QToolButton(headingRow);
  m_explorerNew->setObjectName(QStringLiteral("assistantExplorerNew"));
  m_explorerNew->setText(QStringLiteral("+"));
  m_explorerNew->setToolTip(tr("New chat"));
  m_explorerNew->setCursor(Qt::PointingHandCursor);
  m_explorerNew->setFocusPolicy(Qt::NoFocus);
  m_explorerNew->setAutoRaise(true);
  m_explorerNew->setFixedSize(22, 22);

  headingLayout->addWidget(heading, 1);
  headingLayout->addWidget(m_explorerNew, 0);

  m_explorerList = new QListWidget(panel);
  m_explorerList->setObjectName(QStringLiteral("assistantExplorerList"));
  m_explorerList->setFrameShape(QFrame::NoFrame);
  m_explorerList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_explorerList->setUniformItemSizes(false);
  m_explorerList->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_explorerList->setContextMenuPolicy(Qt::CustomContextMenu);
  m_explorerList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_explorerList->setWordWrap(true);
  m_explorerList->setTextElideMode(Qt::ElideRight);

  layout->addWidget(headingRow, 0);
  layout->addWidget(m_explorerList, 1);

  connect(m_explorerNew, &QToolButton::clicked, this,
          &AssistantShell::onNewChatClicked);
  connect(m_explorerList, &QListWidget::itemSelectionChanged, this,
          &AssistantShell::onExplorerSelectionChanged);
  connect(m_explorerList, &QListWidget::itemChanged, this,
          &AssistantShell::onExplorerItemChanged);
  connect(m_explorerList, &QListWidget::customContextMenuRequested, this,
          &AssistantShell::onExplorerContextMenu);

  return panel;
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

  m_newChat = new QPushButton(tr("New chat"), m_header);
  m_newChat->setObjectName(QStringLiteral("assistantShellNewChat"));
  m_newChat->setCursor(Qt::PointingHandCursor);
  m_newChat->setFocusPolicy(Qt::NoFocus);

  auto *layout = new QHBoxLayout(m_header);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(16);
  layout->addWidget(m_title);
  layout->addWidget(m_status);
  layout->addStretch(1);
  layout->addWidget(m_newChat);

  connect(m_newChat, &QPushButton::clicked, this,
          &AssistantShell::onNewChatClicked);

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
  m_chatLayout->setContentsMargins(0, 0, 0, 16);
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

  m_attach = makeSpeechButton(QStringLiteral("assistantShellAttach"),
                              QStringLiteral("+"),
                              tr("Attach a file to memory."));
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
  layout->addWidget(m_attach);
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
  connect(m_attach, &QToolButton::clicked, this,
          &AssistantShell::onAttachClicked);
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
  if (!m_hero || !m_chatLayout) {
    return;
  }

  const bool empty = m_tree && m_tree->roots().isEmpty();

  if (empty) {
    if (m_chatLayout->indexOf(m_hero) < 0)
      m_chatLayout->insertWidget(0, m_hero);

    m_hero->show();
  } else {
    if (m_chatLayout->indexOf(m_hero) >= 0)
      m_chatLayout->removeWidget(m_hero);

    m_hero->hide();
  }
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
  if (!m_input) {
    return;
  }

  const QString text = m_input->text().trimmed();

  if (text.isEmpty()) {
    return;
  }

  m_input->clear();

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

void AssistantShell::onAttachClicked() {
  if (!m_assistant) {
    return;
  }

  const QString path = QFileDialog::getOpenFileName(
      this, tr("Attach a file to memory"), QString(),
      tr("Documents (*.md *.pdf *.html *.htm *.docx *.pptx *.epub);;"
         "All files (*)"));

  if (path.isEmpty()) {
    return;
  }

  bool ok = false;

  const QString suggested =
      QFileInfo(path).completeBaseName().toLower().simplified().replace(
          QChar(' '), QChar('-'));

  const QString topic = QInputDialog::getText(
      this, tr("Attach to memory"),
      tr("Topic name (used as the file name under memories/topics/):"),
      QLineEdit::Normal, suggested, &ok);

  if (!ok) {
    return;
  }

  m_assistant->importToMemory(path, topic);
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

  m_mindScene->setAssistantRoot(root);
  m_mindScene->refresh();
  m_mindView->refresh();
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

  m_activeReplies.insert(nodeId);

  setBusy(!m_activeReplies.isEmpty());
}

void AssistantShell::onAssistantChunk(const QString &nodeId,
                                      const QString &text) {
  if (!m_tree || nodeId.isEmpty()) {
    return;
  }

  m_tree->appendText(nodeId, text);

  updateWidget(nodeId);
}

void AssistantShell::onAssistantTurnFinished(const QString &nodeId) {
  if (nodeId.isEmpty()) {
    return;
  }

  m_activeReplies.remove(nodeId);

  setBusy(!m_activeReplies.isEmpty());

  if (m_activeReplies.isEmpty() && m_input && isVisible()) {
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

  m_tree->appendJob(kind, title, detail, jobId, nodeId);
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

  QString parent;

  for (const QString &reply : m_activeReplies) {
    parent = reply;
    break;
  }

  if (parent.isEmpty()) {
    m_tree->appendText(ChatNode::Kind::Status, text, QString());
  } else {
    m_tree->appendText(ChatNode::Kind::Status, text, parent);
  }
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
    } else if (m_activeReplies.isEmpty()) {
      if (m_status) {
        m_status->setText(tr("Idle"));
      }
    }
  }

  if (!m_loading && m_store) {
    m_store->save(m_tree);
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
            if (!m_assistant) {
              return;
            }

            if (jobId.isEmpty()) {
              m_assistant->abortAll();
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

  m_activeReplies.clear();

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

  if (m_abort) {
    m_abort->setVisible(busy);
  }
}

void AssistantShell::reloadExplorer() {
  if (!m_explorerList || !m_store) {
    return;
  }

  m_explorerList->blockSignals(true);
  m_explorerList->clear();

  const QVector<ChatTreeStore::Entry> entries = m_store->listSegments();

  QDate lastDate;

  for (const ChatTreeStore::Entry &entry : entries) {
    if (entry.date != lastDate) {
      lastDate = entry.date;

      auto *header = new QListWidgetItem(
          entry.date.toString(QStringLiteral("yyyy-MM-dd")),
          m_explorerList);

      header->setFlags(Qt::NoItemFlags);
      header->setData(Qt::UserRole, QStringLiteral("__header__"));
      header->setData(kExplorerEntryRole, QString());

      QFont headerFont = header->font();
      headerFont.setPointSizeF(qMax(7.0, headerFont.pointSizeF() - 1.5));
      headerFont.setCapitalization(QFont::AllUppercase);
      headerFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
      headerFont.setWeight(QFont::DemiBold);
      header->setFont(headerFont);

      header->setForeground(m_tokens.textSubtle);
      header->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      header->setSizeHint(QSize(0, 30));
    }

    auto *item = new QListWidgetItem(entry.name, m_explorerList);
    item->setData(kExplorerEntryRole, entry.absolutePath);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                   Qt::ItemIsEditable);
    item->setToolTip(entry.name);

    if (entry.absolutePath == m_store->currentPath()) {
      item->setSelected(true);
      m_explorerList->setCurrentItem(item);
    }
  }

  m_explorerList->blockSignals(false);
}

void AssistantShell::beginInlineRename(QListWidgetItem *item) {
  if (!item || !m_explorerList || isHeaderItem(item))
    return;

  m_explorerList->setCurrentItem(item);
  m_explorerList->editItem(item);
}

void AssistantShell::onNewChatClicked() {
  if (!m_store || !m_tree) {
    return;
  }

  m_store->save(m_tree);

  m_store->beginNewSegment();

  rememberCurrentSegment();

  m_loading = true;
  m_tree->clear();
  m_loading = false;

  if (m_assistant) {
    m_assistant->setCurrentChat(m_tree);
  }

  m_activeReplies.clear();
  setBusy(false);

  reloadExplorer();

  if (m_explorerDock) {
    m_explorerDock->showDock();
  }

  if (m_explorerList) {
    for (int i = 0; i < m_explorerList->count(); ++i) {
      QListWidgetItem *row = m_explorerList->item(i);

      if (!row || isHeaderItem(row))
        continue;

      if (row->data(kExplorerEntryRole).toString() == m_store->currentPath()) {
        beginInlineRename(row);
        break;
      }
    }
  }

  focusPrompt();
}

void AssistantShell::onExplorerSelectionChanged() {
  if (!m_explorerList || !m_store || !m_tree) {
    return;
  }

  const QList<QListWidgetItem *> selected = m_explorerList->selectedItems();

  if (selected.isEmpty()) {
    return;
  }

  QListWidgetItem *first = selected.first();

  if (isHeaderItem(first))
    return;

  const QString path = first->data(kExplorerEntryRole).toString();

  if (path.isEmpty())
    return;

  if (path == m_store->currentPath())
    return;

  loadSegmentIntoTree(path);
}

void AssistantShell::loadSegmentIntoTree(const QString &absolutePath) {
  if (!m_store || !m_tree)
    return;

  if (!m_store->openSegment(absolutePath))
    return;

  rememberCurrentSegment();

  m_loading = true;
  m_store->load(m_tree);
  m_loading = false;

  if (m_assistant) {
    m_assistant->setCurrentChat(m_tree);
  }

  updateHeroVisibility();
  scrollToBottom();
}

void AssistantShell::onExplorerItemChanged(QListWidgetItem *item) {
  if (!item || !m_store || !m_explorerList)
    return;

  if (isHeaderItem(item))
    return;

  const QString path = item->data(kExplorerEntryRole).toString();

  if (path.isEmpty())
    return;

  const QString typed = item->text().trimmed();

  const QString currentName =
      QFileInfo(path).completeBaseName();

  if (typed.isEmpty() || typed == currentName) {
    item->setText(currentName);
    return;
  }

  const QString newPath = m_store->renameSegment(path, typed);

  if (newPath.isEmpty()) {
    item->setText(currentName);
    return;
  }

  const QString finalName = QFileInfo(newPath).completeBaseName();

  m_explorerList->blockSignals(true);
  item->setText(finalName);
  item->setData(kExplorerEntryRole, newPath);
  m_explorerList->blockSignals(false);

  rememberCurrentSegment();
}

void AssistantShell::onExplorerContextMenu(const QPoint &pos) {
  if (!m_explorerList || !m_store || !m_tree)
    return;

  QListWidgetItem *item = m_explorerList->itemAt(pos);

  if (!item || isHeaderItem(item))
    return;

  const QString path = item->data(kExplorerEntryRole).toString();

  if (path.isEmpty())
    return;

  QMenu menu(m_explorerList);

  QAction *rename = menu.addAction(tr("Rename"));
  QAction *remove = menu.addAction(tr("Delete"));

  QAction *chosen = menu.exec(m_explorerList->viewport()->mapToGlobal(pos));

  if (!chosen)
    return;

  if (chosen == rename) {
    beginInlineRename(item);
    return;
  }

  if (chosen != remove)
    return;

  const QString name = QFileInfo(path).completeBaseName();

  const auto reply = QMessageBox::question(
      this, tr("Delete chat"),
      tr("Delete \u201c%1\u201d? This cannot be undone.").arg(name),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes)
    return;

  const bool wasCurrent = (path == m_store->currentPath());

  if (!m_store->deleteSegment(path))
    return;

  if (!wasCurrent) {
    reloadExplorer();
    return;
  }

  m_loading = true;
  m_tree->clear();
  m_loading = false;

  m_activeReplies.clear();
  setBusy(false);

  const QVector<ChatTreeStore::Entry> remaining = m_store->listSegments();

  if (remaining.isEmpty()) {
    m_store->beginNewSegment();
  } else {
    loadSegmentIntoTree(remaining.first().absolutePath);
  }

  if (m_assistant) {
    m_assistant->setCurrentChat(m_tree);
  }

  rememberCurrentSegment();

  reloadExplorer();
  updateHeroVisibility();
}