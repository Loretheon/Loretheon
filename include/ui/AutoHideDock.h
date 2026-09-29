#pragma once

#include <QString>
#include <QWidget>

class QTimer;

class AutoHideDock : public QWidget {
  Q_OBJECT

public:
  enum class Edge { Left, Right, Top, Bottom };

  static constexpr int StripWidth = 20;
  static constexpr int MinDockLength = 180;
  static constexpr int SlideDurationMs = 120;

  static constexpr int ResizeHandleWidth = 8;

  explicit AutoHideDock(Edge edge, const QString &settingsPrefix,
                        QWidget *parent = nullptr);

  Edge edge() const { return m_edge; }

  Qt::Orientation orientation() const;

  void setContent(QWidget *content);
  QWidget *content() const { return m_content; }

  void setDockLength(int length);
  int dockLength() const { return m_dockLength; }

  void setMaxDockLength(int length);
  int maxDockLength() const { return m_maxDockLength; }

  void setPreferredContentLength(int length);
  int preferredContentLength() const { return m_preferredContentLength; }

  void fitToContentLength();

  int naturalLength() const { return m_naturalLength; }

  bool hasUserOverrideLength() const { return m_userOverrideLength; }
  void clearUserOverrideLength();

  // The dock's "pin" state has three values. Unpinned means hover
  // opens and leaving closes. PinnedOpen means always expanded.
  // PinnedClosed means always collapsed and refuses to open until
  // unpinned.
  enum class Pin { None, Open, Closed };

  Pin pin() const { return m_pin; }
  void setPin(Pin pin);

  bool isExpanded() const { return m_expanded; }
  void showDock();
  void hideDock();

  void showDockImmediate();
  void hideDockImmediate();

  void scheduleHide();
  void cancelHide();

  void setHideDelayMs(int ms);
  int hideDelayMs() const;

signals:
  void dockLengthChanged(int length);
  void expandedChanged(bool expanded);
  void pinChanged(Pin pin);

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  QString lengthSettingsKey() const;
  QString overrideSettingsKey() const;
  QString pinSettingsKey() const;
  QString expandedSettingsKey() const;

  void loadPersistedState();
  void persistLength();
  void persistOverride();
  void persistPin();
  void persistExpanded();

  bool mouseIsOverDock() const;
  bool mouseIsOverStrip() const;
  bool mouseIsOverResizeHandle(const QPoint &localPos) const;

  void updateCursor(const QPoint &pos);
  void updateStripGeometry();
  void updateContentGeometry();

  void refreshStripVisuals();
  void showStripContextMenu(const QPoint &globalPos);

  static constexpr int kDefaultHideDelayMs = 1000;

  Edge m_edge;
  QString m_settingsPrefix;

  QWidget *m_content = nullptr;
  QWidget *m_strip = nullptr;
  QWidget *m_stripChevron = nullptr;

  QTimer *m_hideTimer = nullptr;

  int m_dockLength = 280;
  int m_maxDockLength = 100000;
  int m_preferredContentLength = 0;
  int m_naturalLength = 0;
  bool m_expanded = true;
  Pin m_pin = Pin::None;
  bool m_userOverrideLength = false;

  bool m_resizing = false;
  int m_resizeStartLength = 0;
  int m_resizeStartGlobalPos = 0;

  bool m_hoveringHandle = false;

  // Whether the strip is currently hovered. Drives the strip's
  // paintEvent highlight.
  bool m_hoveringStrip = false;
};