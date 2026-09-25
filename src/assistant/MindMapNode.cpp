#include "../../include/assistant/MindMapNode.h"

#include "../../include/app/theme/ThemeRegistry.h"

#include <QFontMetricsF>
#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QPainterPath>

MindMapNode::MindMapNode(Kind kind, const QString &title,
                         const QString &detail, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_kind(kind), m_title(title),
      m_detail(detail) {
  setAcceptHoverEvents(true);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
}

void MindMapNode::addChild(MindMapNode *child) {
  if (!child) {
    return;
  }

  child->setParentItem(this);
  child->setDepth(m_depth + 1);
  m_children.append(child);
}

QRectF MindMapNode::boundingRect() const {
  QFont titleFont;
  titleFont.setPointSizeF(11.0);
  titleFont.setBold(m_kind == Kind::Root);

  const QFontMetricsF metrics(titleFont);

  qreal width = metrics.horizontalAdvance(m_title) + kPaddingX * 2.0;

  if (!m_detail.isEmpty()) {
    QFont detailFont;
    detailFont.setPointSizeF(9.0);

    const QFontMetricsF detailMetrics(detailFont);
    width = qMax(width,
                 detailMetrics.horizontalAdvance(m_detail) +
                     kPaddingX * 2.0);
  }

  width = qMax(width, kMinWidth);

  qreal height = metrics.height() + kPaddingY * 2.0;

  if (!m_detail.isEmpty()) {
    QFont detailFont;
    detailFont.setPointSizeF(9.0);

    height += QFontMetricsF(detailFont).height() + 2.0;
  }

  return QRectF(-width / 2.0, -height / 2.0, width, height);
}

void MindMapNode::paint(QPainter *painter,
                        const QStyleOptionGraphicsItem *option,
                        QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  const ThemeTokens tokens = ThemeRegistry::instance().tokens(
      ThemeRegistry::instance().activeTheme());

  QColor fill;
  QColor border;
  QColor text;

  switch (m_kind) {
  case Kind::Root:
    fill = tokens.accent;
    border = tokens.accentHover;
    text = tokens.accentFg;
    break;
  case Kind::Profile:
    fill = tokens.surface2;
    border = tokens.accentMuted;
    text = tokens.text;
    break;
  case Kind::File:
    fill = tokens.surface1;
    border = tokens.border;
    text = tokens.text;
    break;
  case Kind::Topic:
    fill = tokens.surface1;
    border = tokens.hintCool;
    text = tokens.text;
    break;
  case Kind::Fact:
    fill = tokens.surface0;
    border = tokens.border;
    text = tokens.textMuted;
    break;
  }

  qreal opacity = 1.0;

  if (m_focused) {
    opacity = 1.0;
    border = tokens.accent;
  } else if (m_highlighted) {
    opacity = 0.85;
  } else {
    opacity = 0.35;
  }

  painter->setOpacity(opacity);

  const QRectF rect = boundingRect().adjusted(0.5, 0.5, -0.5, -0.5);

  QPainterPath path;
  path.addRoundedRect(rect, kRadius, kRadius);

  painter->setPen(QPen(border, m_focused ? 2.0 : 1.0));
  painter->setBrush(fill);
  painter->drawPath(path);

  QFont titleFont;
  titleFont.setPointSizeF(11.0);
  titleFont.setBold(m_kind == Kind::Root);

  painter->setFont(titleFont);
  painter->setPen(text);

  QRectF titleRect = rect;
  titleRect.setLeft(rect.left() + kPaddingX);
  titleRect.setRight(rect.right() - kPaddingX);

  if (!m_detail.isEmpty()) {
    titleRect.setHeight(rect.height() / 2.0);
  }

  painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_title);

  if (!m_detail.isEmpty()) {
    QFont detailFont;
    detailFont.setPointSizeF(9.0);

    painter->setFont(detailFont);

    QRectF detailRect = rect;
    detailRect.setLeft(rect.left() + kPaddingX);
    detailRect.setRight(rect.right() - kPaddingX);
    detailRect.setTop(titleRect.bottom());
    detailRect.setHeight(rect.height() / 2.0);

    QColor detailColor = text;
    detailColor.setAlpha(160);
    painter->setPen(detailColor);

    painter->drawText(detailRect,
                      Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                      m_detail);
  }
}

void MindMapNode::setFocused(bool focused) {
  if (m_focused == focused) {
    return;
  }

  m_focused = focused;
  update();
}

void MindMapNode::setExpanded(bool expanded) {
  if (m_expanded == expanded) {
    return;
  }

  m_expanded = expanded;
  update();
}

void MindMapNode::setHighlighted(bool highlighted) {
  if (m_highlighted == highlighted) {
    return;
  }

  m_highlighted = highlighted;
  update();
}

QPointF MindMapNode::childAnchor() const {
  const QRectF rect = boundingRect();
  return QPointF(rect.right(), 0.0);
}

QPointF MindMapNode::parentAnchor() const {
  const QRectF rect = boundingRect();
  return QPointF(rect.left(), 0.0);
}

void MindMapNode::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
  Q_UNUSED(event);
  update();
}

void MindMapNode::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
  Q_UNUSED(event);
  update();
}