#include "../../include/avatar/AvatarMoveGrip.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

namespace {

constexpr int kThickness = 8;

} // namespace

AvatarMoveGrip::AvatarMoveGrip(Edge edge, QWidget *parent)
    : QWidget(parent), m_edge(edge) {
  setAttribute(Qt::WA_NoSystemBackground);
  setMouseTracking(true);

  switch (edge) {
  case Edge::Top:
  case Edge::Bottom:
    setCursor(Qt::SizeAllCursor);
    setFixedHeight(kThickness);
    break;
  case Edge::Left:
  case Edge::Right:
    setCursor(Qt::SizeAllCursor);
    setFixedWidth(kThickness);
    break;
  }
}

int AvatarMoveGrip::gripThickness() { return kThickness; }

void AvatarMoveGrip::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::RightButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  qDebug() << "[AvatarMoveGrip] press on edge" << static_cast<int>(m_edge)
           << "global:" << event->globalPosition().toPoint()
           << "size:" << size();

  m_dragging = true;
  m_dragOrigin = event->globalPosition().toPoint();
  event->accept();
}

void AvatarMoveGrip::mouseMoveEvent(QMouseEvent *event) {
  if (!m_dragging) {
    QWidget::mouseMoveEvent(event);
    return;
  }

  const QPoint now = event->globalPosition().toPoint();
  const QPoint delta = now - m_dragOrigin;
  m_dragOrigin = now;

  emit moveBy(delta);
  event->accept();
}

void AvatarMoveGrip::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::RightButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  if (m_dragging) {
    m_dragging = false;
    emit dragFinished();
  }
  event->accept();
}

void AvatarMoveGrip::enterEvent(QEnterEvent *event) {
  m_hovered = true;
  update();
  QWidget::enterEvent(event);
}

void AvatarMoveGrip::leaveEvent(QEvent *event) {
  m_hovered = false;
  update();
  QWidget::leaveEvent(event);
}

void AvatarMoveGrip::paintEvent(QPaintEvent *) {
  if (!m_hovered) {
    return;
  }

  QPainter painter(this);
  painter.fillRect(rect(), QColor(255, 255, 255, 40));
}