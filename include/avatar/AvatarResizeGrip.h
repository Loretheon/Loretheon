#pragma once

#include <QPoint>
#include <QSize>
#include <QWidget>

// A small resize handle drawn in one corner of its parent. The parent
// is expected to be a free-floating widget whose geometry the grip is
// allowed to change. Dragging the grip emits dragged() with a new
// size for the parent, aspect-locked to the parent's current aspect
// ratio, clamped to [minSize, maxSize].
//
// The grip does not resize its parent itself. It reports the size;
// the parent decides what to do with it. That keeps the grip free of
// any knowledge of the widget it is attached to.
class AvatarResizeGrip : public QWidget {
  Q_OBJECT

public:
  enum class Corner {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
  };

  AvatarResizeGrip(Corner corner, QWidget *parent);

  // Clamp bounds applied during a drag. Set once after construction.
  void setSizeBounds(const QSize &minSize, const QSize &maxSize);

  // The side length of the square grip, in pixels.
  static int gripSize();

  signals:
    // Emitted continuously during a drag. newSize is the proposed size
    // for the parent, already aspect-locked and clamped. The parent
    // should apply it and reposition itself so that the corner opposite
    // the grip stays fixed.
    void dragged(const QSize &newSize, AvatarResizeGrip::Corner corner);

  // Emitted once when the mouse button is released. Callers use this
  // to persist the final size.
  void dragFinished();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  QSize lockedSize(const QSize &raw) const;

  Corner m_corner;
  QPoint m_dragOrigin;
  QSize m_originSize;
  QSize m_minSize = QSize(120, 180);
  QSize m_maxSize = QSize(640, 960);
  bool m_dragging = false;
  bool m_hovered = false;
};