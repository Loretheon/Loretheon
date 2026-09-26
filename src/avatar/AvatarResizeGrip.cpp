#include "../../include/avatar/AvatarResizeGrip.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

namespace {

constexpr int kGripSize = 16;
constexpr int kInset = 2;

} // namespace

AvatarResizeGrip::AvatarResizeGrip(Corner corner, QWidget *parent)
    : QWidget(parent), m_corner(corner) {
  setFixedSize(kGripSize, kGripSize);
  setCursor(corner == Corner::TopLeft || corner == Corner::BottomRight
                ? Qt::SizeFDiagCursor
                : Qt::SizeBDiagCursor);
  setAttribute(Qt::WA_TransparentForMouseEvents, false);
  setAttribute(Qt::WA_NoSystemBackground);
  setMouseTracking(true);
}

int AvatarResizeGrip::gripSize() { return kGripSize; }

void AvatarResizeGrip::setSizeBounds(const QSize &minSize,
                                     const QSize &maxSize) {
  m_minSize = minSize;
  m_maxSize = maxSize;
}

QSize AvatarResizeGrip::lockedSize(const QSize &raw) const {
  // The parent's aspect ratio is fixed. We derive it from the origin
  // size recorded at press time, so the ratio does not drift during a
  // drag even if the parent is momentarily at a clamped value.
  if (m_originSize.height() <= 0 || m_originSize.width() <= 0) {
    return raw;
  }

  const double aspect =
      static_cast<double>(m_originSize.width()) / m_originSize.height();

  // Pick whichever axis the user moved more, and derive the other from
  // it. That makes the drag feel like it follows the mouse on the
  // dominant axis.
  double w = raw.width();
  double h = raw.height();

  if (qAbs(raw.width() - m_originSize.width()) >=
      qAbs(raw.height() - m_originSize.height())) {
    h = w / aspect;
  } else {
    w = h * aspect;
  }

  // Clamp. Clamping one axis and recomputing the other preserves the
  // aspect exactly.
  if (w < m_minSize.width()) {
    w = m_minSize.width();
    h = w / aspect;
  }
  if (h < m_minSize.height()) {
    h = m_minSize.height();
    w = h * aspect;
  }
  if (w > m_maxSize.width()) {
    w = m_maxSize.width();
    h = w / aspect;
  }
  if (h > m_maxSize.height()) {
    h = m_maxSize.height();
    w = h * aspect;
  }

  return QSize(qRound(w), qRound(h));
}

void AvatarResizeGrip::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  QWidget *p = parentWidget();
  if (!p) {
    return;
  }

  m_dragging = true;
  m_dragOrigin = event->globalPosition().toPoint();
  m_originSize = p->size();

  event->accept();
}

void AvatarResizeGrip::mouseMoveEvent(QMouseEvent *event) {
  if (!m_dragging) {
    QWidget::mouseMoveEvent(event);
    return;
  }

  const QPoint delta =
      event->globalPosition().toPoint() - m_dragOrigin;

  // How the delta maps to a size depends on which corner the grip is
  // in. For a bottom-right grip, moving down and right grows the
  // widget. For a top-left grip, moving up and left grows it, so the
  // sign on each axis flips.
  int dx = delta.x();
  int dy = delta.y();

  switch (m_corner) {
  case Corner::TopLeft:
    dx = -dx;
    dy = -dy;
    break;
  case Corner::TopRight:
    dy = -dy;
    break;
  case Corner::BottomLeft:
    dx = -dx;
    break;
  case Corner::BottomRight:
    break;
  }

  const QSize raw(m_originSize.width() + dx, m_originSize.height() + dy);
  const QSize locked = lockedSize(raw);

  emit dragged(locked, m_corner);

  event->accept();
}

void AvatarResizeGrip::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  if (m_dragging) {
    m_dragging = false;
    emit dragFinished();
  }

  event->accept();
}

void AvatarResizeGrip::enterEvent(QEnterEvent *event) {
  m_hovered = true;
  update();
  QWidget::enterEvent(event);
}

void AvatarResizeGrip::leaveEvent(QEvent *event) {
  m_hovered = false;
  update();
  QWidget::leaveEvent(event);
}

void AvatarResizeGrip::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QColor base = m_hovered ? QColor(255, 255, 255, 180)
                                : QColor(255, 255, 255, 90);

  painter.setPen(Qt::NoPen);
  painter.setBrush(base);

  // Three short strokes radiating from the corner the grip is in,
  // like the classic OS resize affordance.
  const int w = width();
  const int h = height();

  auto stroke = [&](int x1, int y1, int x2, int y2) {
    painter.drawRoundedRect(
        QRectF(qMin(x1, x2), qMin(y1, y2),
               qAbs(x2 - x1) + kInset, qAbs(y2 - y1) + kInset),
        1.0, 1.0);
  };

  const bool right =
      m_corner == Corner::TopRight || m_corner == Corner::BottomRight;
  const bool bottom =
      m_corner == Corner::BottomLeft || m_corner == Corner::BottomRight;

  const int cx = right ? w - 2 : 2;
  const int cy = bottom ? h - 2 : 2;

  const int dirX = right ? -1 : 1;
  const int dirY = bottom ? -1 : 1;

  for (int i = 1; i <= 3; ++i) {
    const int len = i * 3;
    stroke(cx + dirX * len, cy, cx, cy);
    stroke(cx, cy + dirY * len, cx, cy);
  }
}