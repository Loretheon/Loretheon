#pragma once

#include <QWidget>

class ConductorBoard;
class ConductorQueue;
class ConductorRoster;
class DependencyGraph;
class QPropertyAnimation;
class QPushButton;

class ConductorDock : public QWidget {
  Q_OBJECT

public:
  explicit ConductorDock(QWidget *parent);

  void setQueue(ConductorQueue *queue);
  void setRoster(ConductorRoster *roster);
  void setDependencies(DependencyGraph *graph);

  // The session folder used to persist the board's splitter geometry
  // and generated exports. Empty means do not persist.
  void setSessionFolder(const QString &folder);

  ConductorBoard *board() const { return m_board; }

  bool isOpen() const { return m_open; }

  void setHeightFraction(double fraction);

public slots:
  void open();
  void close();
  void toggle();

  signals:
    void opened();
  void closed();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  void reposition();
  void animateTo(int targetHeight);

  ConductorBoard *m_board = nullptr;

  QPushButton *m_closeButton = nullptr;

  QPropertyAnimation *m_animation = nullptr;

  bool m_open = false;
  double m_heightFraction = 0.6;

  bool m_resizing = false;
  int m_resizeStartY = 0;
  int m_resizeStartHeight = 0;

  static constexpr int kHandleHeight = 8;
  static constexpr int kMinimumHeight = 160;
  static constexpr int kAnimationMs = 180;
};