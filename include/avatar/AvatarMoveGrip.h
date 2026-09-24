#pragma once

#include <QPoint>
#include <QWidget>

class AvatarMoveGrip : public QWidget {
  Q_OBJECT

public:
  enum class Edge {
    Top,
    Right,
    Bottom,
    Left,
  };

  AvatarMoveGrip(Edge edge, QWidget *parent);

  // How thick the strip is, in pixels.
  static int gripThickness();

  signals:
    void moveBy(const QPoint &delta);
  void dragFinished();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  Edge m_edge;
  QPoint m_dragOrigin;
  bool m_dragging = false;
  bool m_hovered = false;
};