#pragma once

#include "ChatTree.h"
#include "LoreAssistant.h"
#include "ThemeAware.h"

#include <QHash>
#include <QWidget>

class AvatarWidget;
class ChatNodeWidget;
class LoreAssistant;
class MindMapScene;
class MindMapView;
class OverseerSessionManager;
class SpeechController;

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
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
  void onSpeechStateChanged();
  void onTranscribed(const QString &text);
  void onLiveTranscribed(const QString &text, bool isFinal);
  void onPolicyChanged(int index);
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

private:
  void buildUi();
  QWidget *buildHeader();
  QWidget *buildChatTab();
  QWidget *buildMindTab();
  QWidget *buildControls();
  QWidget *buildHero();

  void applySpeechButtonState(QToolButton *button, bool active);

  void appendTopLevelWidget(const QString &nodeId);
  void updateWidget(const QString &nodeId);
  void scrollToBottom();

  void placeAvatarOnce();
  void updateHeroVisibility();

  LoreAssistant *m_assistant = nullptr;
  SpeechController *m_speech = nullptr;
  AvatarWidget *m_avatar = nullptr;
  OverseerSessionManager *m_overseer = nullptr;

  ChatTree *m_tree = nullptr;

  QWidget *m_header = nullptr;
  QLabel *m_title = nullptr;
  QLabel *m_status = nullptr;
  QComboBox *m_policy = nullptr;

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

  QString m_activeReplyNode;

  bool m_busy = false;

  bool m_avatarPlaced = false;

  ThemeTokens m_tokens;
};