#pragma once

#include "LoreAssistant.h"
#include "ThemeAware.h"

#include <QPoint>
#include <QRect>
#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QScreen;
class QTextEdit;
class QToolButton;

class LoreAssistant;
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

  void appendUserMessage(const QString &text);
  void appendAssistantChunk(const QString &text);
  void appendStatusMessage(const QString &text);
  void clearTranscript();

  void setBusy(bool busy);
  bool isBusy() const { return m_busy; }

signals:
  void messageSubmitted(const QString &text);
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
  void onDictateClicked();
  void onLiveDictateClicked();
  void onReadAloudClicked();
  void onCloseClicked();
  void onSpeechStateChanged();
  void onTranscribed(const QString &text);
  void onLiveTranscribed(const QString &text, bool isFinal);
  void onPolicyChanged(int index);

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
  void applySpeechButtonState(QToolButton *button, bool active);
  void renderTranscript();

  DragKind bandFor(const QPoint &localPos) const;
  bool isDragPoint(const QPoint &pos) const;
  void applyResize(const QPoint &globalDelta);

  QScreen *screenForCurrentPosition() const;
  QRect currentScreenGeometry() const;

  void restoreSavedPosition();
  void savePosition() const;

  QFrame *m_card = nullptr;
  QWidget *m_header = nullptr;
  QWidget *m_controls = nullptr;

  QLabel *m_title = nullptr;
  QComboBox *m_policy = nullptr;
  QTextEdit *m_transcript = nullptr;
  QLineEdit *m_input = nullptr;
  QPushButton *m_send = nullptr;
  QPushButton *m_close = nullptr;
  QToolButton *m_dictate = nullptr;
  QToolButton *m_live = nullptr;
  QToolButton *m_readAloud = nullptr;

  LoreAssistant *m_assistant = nullptr;
  SpeechController *m_speech = nullptr;

  ThemeTokens m_tokens;

  struct Entry {
    enum class Kind { User, Assistant, Status };
    Kind kind = Kind::Assistant;
    QString text;
  };

  QVector<Entry> m_entries;

  DragKind m_drag = DragKind::None;
  QPoint m_dragOriginGlobal;
  QRect m_originGeometry;

  bool m_open = false;
  bool m_busy = false;
};