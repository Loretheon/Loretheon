#pragma once

#include <QGraphicsItem>
#include <QList>
#include <QPointF>
#include <QString>

class QTextDocument;

class MindMapNode : public QGraphicsItem {
public:
  enum class Kind {
    Root,
    Hub,
    ProfileFile,
    MemoryTopic,
    MemorySession,
    Fact,
    OverseerSession,
    CrossLink,
  };

  explicit MindMapNode(Kind kind, const QString &title,
                       const QString &detail = QString(),
                       QGraphicsItem *parent = nullptr);
  ~MindMapNode() override;

  Kind kind() const { return m_kind; }
  QString title() const { return m_title; }
  QString detail() const { return m_detail; }

  QString cacheId() const { return m_cacheId; }
  void setCacheId(const QString &id) { m_cacheId = id; }

  void setPath(const QString &path) { m_path = path; }
  QString path() const { return m_path; }

  void setSessionName(const QString &name) { m_sessionName = name; }
  QString sessionName() const { return m_sessionName; }

  void addChild(MindMapNode *child);
  const QList<MindMapNode *> &children() const { return m_children; }

  void addCrossLink(MindMapNode *peer);
  const QList<MindMapNode *> &crossLinks() const { return m_crossLinks; }

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;

  void setFocused(bool focused);
  bool isFocused() const { return m_focused; }

  void setHighlighted(bool highlighted);
  bool isHighlighted() const { return m_highlighted; }

  void setDimmed(bool dimmed);
  bool isDimmed() const { return m_dimmed; }

  void setPinned(bool pinned) { m_pinned = pinned; }
  bool isPinned() const { return m_pinned; }

  QPointF childAnchor(const QPointF &toward) const;
  QPointF parentAnchor(const QPointF &toward) const;

  void setDepth(int depth) { m_depth = depth; }
  int depth() const { return m_depth; }

  qreal scaleForKind() const;

  void setVelocity(const QPointF &v) { m_velocity = v; }
  QPointF velocity() const { return m_velocity; }

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
  void ensureDocument() const;
  QRectF computeRect() const;
  qreal preferredContentWidth() const;

  Kind m_kind;
  QString m_title;
  QString m_detail;
  QString m_cacheId;
  QString m_path;
  QString m_sessionName;

  QList<MindMapNode *> m_children;
  QList<MindMapNode *> m_crossLinks;

  bool m_focused = false;
  bool m_highlighted = false;
  bool m_dimmed = false;
  bool m_pinned = false;
  int m_depth = 0;

  QPointF m_velocity;

  mutable QTextDocument *m_document = nullptr;
  mutable qreal m_contentWidth = 0.0;
  mutable QRectF m_cachedRect;
  mutable bool m_cachedValid = false;

  static constexpr qreal kPaddingX = 14.0;
  static constexpr qreal kPaddingY = 9.0;
  static constexpr qreal kRadius = 10.0;
  static constexpr qreal kMinWidth = 130.0;
  static constexpr qreal kRailWidth = 3.0;
};