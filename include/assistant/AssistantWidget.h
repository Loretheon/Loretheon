#pragma once

#include "ChatTree.h"
#include "LoreAssistant.h"
#include "ThemeAware.h"

#include <QHash>
#include <QPoint>
#include <QRect>
#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QScreen;
class QScrollArea;
class QTabWidget;
class QToolButton;
class QVBoxLayout;

class ChatNodeWidget;
class LoreAssistant;
class MindMapScene;
class MindMapView;
class SpeechController;

class AssistantWidget : public QWidget, public ThemeAware {
  Q_OBJECT

public:
  explicit AssistantWidget(QWidget *parent = nullptr);
  ~AssistantWidget() override;

  void setThemeTokens(const ThemeTokens &tokens) override;

  void setAssistant(LoreAssistant *assistant);
  void setSpeechController(SpeechController *speech);

  void open();
  void close();
  void toggle();
  bool isOpen() const { return m_open; }

  void dockTo(QWidget *parent);
  void undock();
  bool isUndocked() const { return false; }

  void positionPanel();

  QString beginUserMessage(const QString &text);
  QString beginAssistantReply(const QString &parentId);
  void appendAssistantChunk(const QString &nodeId, const QString &chunk);
  void appendStatusMessage(const QString &text);
  QString beginJob(const QString &parentId, ChatNode::Kind kind,
                   const QString &title, const QString &detail,
                   const QString &jobId);
  void setJobState(const QString &nodeId, ChatNode::State state);
  void setJobResult(const QString &nodeId, const QString &result);
  void setJobError(const QString &nodeId, const QString &error);

  QString jobIdFor(const QString &jobId) const;

  void setStatus(const QString &status);

  void clearConversation();

  void setBusy(bool busy);
  bool isBusy() const { return m_busy; }

public slots:
  void onAssistantReplyStarted(const QString &nodeId);

signals:
  void messageSubmitted(const QString &text);
  void abortRequested();
  void positionChanged();

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
  void onSubmit();
  void onAbortClicked();
  void onDictateClicked();
  void onLiveDictateClicked();
  void onReadAloudClicked();
  void onCloseClicked();
  void onSpeechStateChanged();
  void onTranscribed(const QString &text);
  void onLiveTranscribed(const QString &text, bool isFinal);
  void onPolicyChanged(int index);
  void onTabChanged(int index);
  void onNodeAdded(const QString &id);
  void onNodeChanged(const QString &id);
  void onTreeCleared();

private:
  enum class DragKind {
    None,
    Move,
    ResizeTopLeft,
    ResizeTopRight,
    ResizeBottomLeft,
    ResizeBottomRight,
    ResizeLeft,
    ResizeRight,
    ResizeTop,
    ResizeBottom,
  };

  void buildUi();
  QWidget *buildChatTab();
  QWidget *buildMindTab();
  void applySpeechButtonState(QToolButton *button, bool active);

  void appendTopLevelWidget(const QString &nodeId);
  void updateWidget(const QString &nodeId);
  void scrollToBottom();

  DragKind bandFor(const QPoint &localPos) const;
  bool isDragPoint(const QPoint &pos) const;
  void applyResize(const QPoint &globalDelta);

  QScreen *screenForCurrentPosition() const;
  QRect currentScreenGeometry() const;

  void restoreSavedPosition();
  void savePosition() const;

  QFrame *m_card = nullptr;
  QWidget *m_header = nullptr;
  QTabWidget *m_tabs = nullptr;

  QLabel *m_title = nullptr;
  QLabel *m_status = nullptr;
  QComboBox *m_policy = nullptr;

  QScrollArea *m_chatScroll = nullptr;
  QWidget *m_chatHost = nullptr;
  QVBoxLayout *m_chatLayout = nullptr;

  // One widget per top-level node, keyed by node id. Top-level nodes
  // are user messages and assistant replies. Everything else lives
  // inside its parent's ChatNodeWidget.
  QHash<QString, ChatNodeWidget *> m_topLevelWidgets;

  QLineEdit *m_input = nullptr;
  QPushButton *m_send = nullptr;
  QPushButton *m_abort = nullptr;
  QPushButton *m_close = nullptr;
  QToolButton *m_dictate = nullptr;
  QToolButton *m_live = nullptr;
  QToolButton *m_readAloud = nullptr;

  MindMapView *m_mindView = nullptr;
  MindMapScene *m_mindScene = nullptr;

  ChatTree *m_tree = nullptr;

  LoreAssistant *m_assistant = nullptr;
  SpeechController *m_speech = nullptr;

  ThemeTokens m_tokens;

  DragKind m_drag = DragKind::None;
  QPoint m_dragOriginGlobal;
  QRect m_originGeometry;

  QString m_activeReplyNode;

  bool m_open = false;
  bool m_busy = false;
};