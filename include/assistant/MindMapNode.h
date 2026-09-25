#pragma once

#include <QGraphicsItem>
#include <QList>
#include <QString>

class MindMapNode : public QGraphicsItem {
public:
  enum class Kind { Root, Profile, File, Topic, Fact };

  explicit MindMapNode(Kind kind, const QString &title,
                       const QString &detail = QString(),
                       QGraphicsItem *parent = nullptr);

  Kind kind() const { return m_kind; }
  QString title() const { return m_title; }
  QString detail() const { return m_detail; }

  void addChild(MindMapNode *child);
  const QList<MindMapNode *> &children() const { return m_children; }

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;

  void setFocused(bool focused);
  bool isFocused() const { return m_focused; }

  void setExpanded(bool expanded);
  bool isExpanded() const { return m_expanded; }

  void setHighlighted(bool highlighted);
  bool isHighlighted() const { return m_highlighted; }

  QPointF childAnchor() const;
  QPointF parentAnchor() const;

  void setDepth(int depth) { m_depth = depth; }
  int depth() const { return m_depth; }

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
  Kind m_kind;
  QString m_title;
  QString m_detail;

  QList<MindMapNode *> m_children;

  bool m_focused = false;
  bool m_expanded = true;
  bool m_highlighted = false;
  int m_depth = 0;

  static constexpr qreal kPaddingX = 12.0;
  static constexpr qreal kPaddingY = 8.0;
  static constexpr qreal kRadius = 6.0;
  static constexpr qreal kMinWidth = 120.0;
};