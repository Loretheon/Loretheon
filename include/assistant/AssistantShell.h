#pragma once

#include "ChatTree.h"
#include "ConversationMode.h"
#include "LoreAssistant.h"
#include "ThemeAware.h"

#include <QHash>
#include <QSet>
#include <QWidget>

class AutoHideDock;
class AvatarWidget;
class ChatNodeWidget;
class ChatTreeStore;
class ConversationMode;
class DockReservation;
class LoreAssistant;
class MindMapScene;
class MindMapView;
class OverseerSessionManager;
class SpeechController;

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPoint;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTabWidget;
class QToolButton;
class QVBoxLayout;

class AssistantShell : public QWidget, public ThemeAware {
  Q_OBJECT

public:
  explicit AssistantShell(QWidget *parent = nullptr);
  ~AssistantShell() override;

  void setThemeTokens(const ThemeTokens &tokens) override;

  void setAssistant(LoreAssistant *assistant);
  void setSpeechController(SpeechController *speech);
  void setOverseerManager(OverseerSessionManager *manager);

  void setAvatar(AvatarWidget *avatar);
  AvatarWidget *avatar() const { return m_avatar; }

  void focusPrompt();

  void setReplyText(const QString &text);

  QString beginUserMessage(const QString &text);

  void setBusy(bool busy);
  bool isBusy() const { return m_busy; }

  void refreshMindMap();

signals:
  void messageSubmitted(const QString &text);
  void dismissed();

protected:
  void showEvent(QShowEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onSubmit();
  void onAbortClicked();
  void onDictateClicked();
  void onLiveDictateClicked();
  void onReadAloudClicked();
  void onAttachClicked();
  void onConversationToggled(bool on);
  void onConversationStateChanged(ConversationMode::State state);
  void onConversationTranscriptChanged(const QString &text);
  void onSpeechStateChanged();
  void onTranscribed(const QString &text);
  void onLiveTranscribed(const QString &text, bool isFinal);
  void onTabChanged(int index);
  void onNodeAdded(const QString &id);
  void onNodeChanged(const QString &id);
  void onTreeCleared();

  void onAssistantReplyStarted(const QString &nodeId);
  void onAssistantChunk(const QString &nodeId, const QString &text);
  void onAssistantTurnFinished(const QString &nodeId);
  void onJobCreated(const QString &jobId, const QString &nodeId,
                    ChatNode::Kind kind, const QString &title,
                    const QString &detail);
  void onJobCompleted(const QString &jobId, const QString &result);
  void onJobFailed(const QString &jobId, const QString &error);
  void onStatusMessage(const QString &text);
  void onStatusChanged(const QString &status);
  void onOverseerSessionListChanged();

  void onNewChatClicked();
  void onExplorerSelectionChanged();
  void onExplorerItemChanged(QListWidgetItem *item);
  void onExplorerContextMenu(const QPoint &pos);

private:
  void buildUi();
  QWidget *buildHeader();
  QWidget *buildChatTab();
  QWidget *buildMindTab();
  QWidget *buildControls();
  QWidget *buildHero();
  QWidget *buildExplorer();

  void applySpeechButtonState(QToolButton *button, bool active);
  void applyConversationEnabled(bool on);

  void appendTopLevelWidget(const QString &nodeId);
  void updateWidget(const QString &nodeId);
  void scrollToBottom();

  void placeAvatarOnce();
  void updateHeroVisibility();

  void reloadExplorer();
  void loadSegmentIntoTree(const QString &absolutePath);
  void beginInlineRename(QListWidgetItem *item);

  void openInitialSegment();
  void rememberCurrentSegment();

  LoreAssistant *m_assistant = nullptr;
  SpeechController *m_speech = nullptr;
  AvatarWidget *m_avatar = nullptr;
  OverseerSessionManager *m_overseer = nullptr;

  ChatTree *m_tree = nullptr;
  ChatTreeStore *m_store = nullptr;

  AutoHideDock *m_explorerDock = nullptr;
  DockReservation *m_explorerReservation = nullptr;
  QListWidget *m_explorerList = nullptr;
  QToolButton *m_explorerNew = nullptr;

  QWidget *m_header = nullptr;
  QLabel *m_title = nullptr;
  QLabel *m_status = nullptr;
  QPushButton *m_newChat = nullptr;

  QTabWidget *m_tabs = nullptr;

  QWidget *m_chatPage = nullptr;
  QScrollArea *m_chatScroll = nullptr;
  QWidget *m_chatHost = nullptr;
  QVBoxLayout *m_chatLayout = nullptr;

  QWidget *m_hero = nullptr;

  QHash<QString, ChatNodeWidget *> m_topLevelWidgets;

  MindMapView *m_mindView = nullptr;
  MindMapScene *m_mindScene = nullptr;

  QWidget *m_controls = nullptr;
  QLineEdit *m_input = nullptr;
  QPushButton *m_send = nullptr;
  QPushButton *m_abort = nullptr;
  QToolButton *m_dictate = nullptr;
  QToolButton *m_live = nullptr;
  QToolButton *m_readAloud = nullptr;
  QToolButton *m_attach = nullptr;
  QToolButton *m_conversation = nullptr;
  QSpinBox *m_silenceSpin = nullptr;

  QSet<QString> m_activeReplies;

  bool m_busy = false;
  bool m_loading = false;
  bool m_avatarPlaced = false;

  ThemeTokens m_tokens;
};