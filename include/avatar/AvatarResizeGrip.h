#pragma once

#include <QPoint>
#include <QSize>
#include <QWidget>

// A small resize handle drawn near a corner of its parent. The parent
// is expected to be the AvatarWidget's *own* parent, not the avatar
// itself, so the grip can straddle the widget's corner without being
// covered by the QQuickWidget's composited surface.
//
// The grip does not resize anything itself. It reports the drag; the
// owner decides what to do.
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

  void setSizeBounds(const QSize &minSize, const QSize &maxSize);

  static int gripSize();

  signals:
    void dragged(const QSize &newSize, AvatarResizeGrip::Corner corner);
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
  QSize m_maxSize = QSize(960, 1440);
  bool m_dragging = false;
  bool m_hovered = false;
};