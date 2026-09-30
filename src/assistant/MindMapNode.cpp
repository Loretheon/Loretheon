#include "../../include/assistant/MindMapNode.h"

#include "../../include/app/theme/ThemeRegistry.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QTextDocument>

namespace {

constexpr qreal kMaxTextWidthRoot = 340.0;
constexpr qreal kMaxTextWidthProfile = 320.0;
constexpr qreal kMaxTextWidthNode = 260.0;

QString htmlEscape(const QString &in) {
  QString out = in;
  out.replace(QChar('&'), QStringLiteral("&amp;"));
  out.replace(QChar('<'), QStringLiteral("&lt;"));
  out.replace(QChar('>'), QStringLiteral("&gt;"));
  return out;
}

QString colorHex(const QColor &c) {
  return c.name(QColor::HexRgb);
}

} // namespace

MindMapNode::MindMapNode(Kind kind, const QString &title,
                         const QString &detail, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_kind(kind), m_title(title),
      m_detail(detail) {
  setAcceptHoverEvents(true);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsMovable, false);


}

MindMapNode::~MindMapNode() {
  delete m_document;
}

void MindMapNode::addChild(MindMapNode *child) {
  if (!child) {
    return;
  }

  child->setParentItem(this);
  child->setDepth(m_depth + 1);
  m_children.append(child);
}

void MindMapNode::addCrossLink(MindMapNode *peer) {
  if (!peer || peer == this) {
    return;
  }

  if (!m_crossLinks.contains(peer)) {
    m_crossLinks.append(peer);
  }

  if (!peer->m_crossLinks.contains(this)) {
    peer->m_crossLinks.append(this);
  }
}

qreal MindMapNode::scaleForKind() const {
  switch (m_kind) {
  case Kind::Root:
    return 1.55;
  case Kind::Hub:
    return 1.25;
  case Kind::ProfileFile:
  case Kind::OverseerSession:
    return 1.05;
  case Kind::MemoryTopic:
    return 1.0;
  case Kind::MemorySession:
    return 0.92;
  case Kind::Fact:
    return 0.82;
  case Kind::CrossLink:
    return 0.7;
  }

  return 1.0;
}

qreal MindMapNode::preferredContentWidth() const {
  const qreal scale = scaleForKind();
  const bool isRoot = m_kind == Kind::Root;

  qreal maxTextWidth;

  switch (m_kind) {
  case Kind::Root:
    maxTextWidth = kMaxTextWidthRoot;
    break;
  case Kind::ProfileFile:
    maxTextWidth = kMaxTextWidthProfile;
    break;
  default:
    maxTextWidth = kMaxTextWidthNode;
    break;
  }

  maxTextWidth *= scale;

  QFont titleFont;
  titleFont.setPointSizeF((isRoot ? 14.0 : 11.0) * scale);
  titleFont.setBold(isRoot || m_kind == Kind::Hub);

  const QFontMetricsF titleMetrics(titleFont);

  const qreal titleWidth = titleMetrics.horizontalAdvance(m_title);

  qreal natural = titleWidth;

  natural = qMin(natural, maxTextWidth);
  natural = qMax(natural, kMinWidth * scale);

  return natural;
}

void MindMapNode::ensureDocument() const {
  const ThemeTokens tokens = ThemeRegistry::instance().tokens(
      ThemeRegistry::instance().activeTheme());

  const qreal scale = scaleForKind();
  const bool isRoot = m_kind == Kind::Root;

  const QColor textColor = tokens.text;

  const qreal contentWidth = preferredContentWidth();
  const qreal titlePt = (isRoot ? 14.0 : 11.0) * scale;

  if (!m_document) {
    m_document = new QTextDocument;
    m_document->setDocumentMargin(0);
  }

  const QString styleSheet =
      QStringLiteral(
          "body { color: %1; }"
          "p { margin: 0; padding: 0; }"
          "h1, h2, h3, h4, h5, h6 {"
          "  color: %1;"
          "  margin: 0;"
          "  padding: 0;"
          "  font-weight: 600; }")
          .arg(colorHex(textColor));

  m_document->setDefaultStyleSheet(styleSheet);

  // The node shows only the title. The body lives in `detail` and is
  // surfaced by the hover panel in MindMapView. Facts carry their
  // text as the title directly and a timestamp as the detail; those
  // are small, so the fact title is all that matters.

  QString html;

  const QString weight =
      (isRoot || m_kind == Kind::Hub) ? QStringLiteral("600")
                                      : QStringLiteral("400");

  html += QStringLiteral(
              "<div style=\"font-size:%1pt;font-weight:%2;"
              "color:%3;margin:0;padding:0;\">%4</div>")
              .arg(titlePt, 0, 'f', 2)
              .arg(weight)
              .arg(colorHex(textColor))
              .arg(htmlEscape(m_title)
                       .replace(QChar('\n'), QStringLiteral("<br/>")));

  m_document->setHtml(html);

  m_document->setTextWidth(contentWidth);

  m_contentWidth = contentWidth;
}

QRectF MindMapNode::computeRect() const {
  ensureDocument();

  const qreal scale = scaleForKind();

  const qreal paddingX = kPaddingX * scale;
  const qreal paddingY = kPaddingY * scale;

  const QSizeF docSize = m_document->size();

  const qreal width = docSize.width() + paddingX * 2.0;
  const qreal height = docSize.height() + paddingY * 2.0;

  return QRectF(-width / 2.0, -height / 2.0, width, height);
}

QRectF MindMapNode::boundingRect() const {
  if (!m_cachedValid) {
    m_cachedRect = computeRect();
    m_cachedValid = true;
  }

  return m_cachedRect;
}

void MindMapNode::paint(QPainter *painter,
                        const QStyleOptionGraphicsItem *option,
                        QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  boundingRect();

  const ThemeTokens tokens = ThemeRegistry::instance().tokens(
      ThemeRegistry::instance().activeTheme());

  const qreal scale = scaleForKind();
  const qreal paddingX = kPaddingX * scale;
  const qreal paddingY = kPaddingY * scale;

  const bool isRoot = m_kind == Kind::Root;

  QColor fill;
  QColor border;
  QColor text;
  QColor rail;

  switch (m_kind) {
  case Kind::Root:
    fill = tokens.accent;
    border = tokens.accentHover;
    text = tokens.accentFg;
    rail = tokens.accentHover;
    break;
  case Kind::Hub:
    fill = tokens.surface2;
    border = tokens.accentMuted;
    text = tokens.text;
    rail = tokens.accent;
    break;
  case Kind::ProfileFile:
    fill = tokens.surface1;
    border = tokens.border;
    text = tokens.text;
    rail = tokens.hintWarm;
    break;
  case Kind::MemoryTopic:
    fill = tokens.surface1;
    border = tokens.border;
    text = tokens.text;
    rail = tokens.hintCool;
    break;
  case Kind::MemorySession:
    fill = tokens.surface0;
    border = tokens.border;
    text = tokens.textMuted;
    rail = tokens.hintCool;
    break;
  case Kind::Fact:
    fill = tokens.surface0;
    border = tokens.border;
    text = tokens.textMuted;
    rail = tokens.hintNeutral;
    break;
  case Kind::OverseerSession:
    fill = tokens.surface1;
    border = tokens.border;
    text = tokens.text;
    rail = tokens.hintWarm;
    break;
  case Kind::CrossLink:
    fill = tokens.surface0;
    border = tokens.divider;
    text = tokens.textSubtle;
    rail = tokens.divider;
    break;
  }

  qreal opacity = 1.0;

  if (m_dimmed) {
    opacity = 0.22;
  } else if (m_focused) {
    opacity = 1.0;
    border = tokens.accent;
  } else if (m_highlighted) {
    opacity = 0.92;
  }

  painter->setOpacity(opacity);

  const QRectF rect = boundingRect().adjusted(0.5, 0.5, -0.5, -0.5);

  QPainterPath path;
  path.addRoundedRect(rect, kRadius * scale, kRadius * scale);

  painter->setPen(QPen(border, m_focused ? 2.0 : 1.0));
  painter->setBrush(fill);
  painter->drawPath(path);

  if (!isRoot) {
    QPainterPath railPath;
    railPath.addRoundedRect(
        QRectF(rect.left() + 2.0, rect.top() + 4.0,
               kRailWidth * scale, rect.height() - 8.0),
        1.5, 1.5);
    painter->setPen(Qt::NoPen);
    painter->setBrush(rail);
    painter->drawPath(railPath);
  }

  if (!m_document) {
    return;
  }

  painter->save();

  painter->setClipRect(rect);

  painter->translate(rect.left() + paddingX,
                     rect.top() + paddingY);

  QAbstractTextDocumentLayout::PaintContext ctx;

  ctx.palette.setColor(QPalette::Text, text);
  ctx.palette.setColor(QPalette::WindowText, text);

  m_document->documentLayout()->draw(painter, ctx);

  painter->restore();
}

void MindMapNode::setFocused(bool focused) {
  if (m_focused == focused) {
    return;
  }

  m_focused = focused;
  update();
}

void MindMapNode::setHighlighted(bool highlighted) {
  if (m_highlighted == highlighted) {
    return;
  }

  m_highlighted = highlighted;
  update();
}

void MindMapNode::setDimmed(bool dimmed) {
  if (m_dimmed == dimmed) {
    return;
  }

  m_dimmed = dimmed;
  update();
}

QPointF MindMapNode::childAnchor(const QPointF &toward) const {
  const QRectF rect = boundingRect();

  const QPointF local = mapFromScene(toward);

  const qreal length = std::hypot(local.x(), local.y());

  if (length < 1e-6) {
    return mapToScene(QPointF(rect.right(), 0.0));
  }

  const qreal halfW = rect.width() / 2.0;
  const qreal halfH = rect.height() / 2.0;

  const qreal scaleX = halfW / std::abs(local.x() + 1e-9);
  const qreal scaleY = halfH / std::abs(local.y() + 1e-9);

  const qreal scale = std::min(scaleX, scaleY);

  const QPointF edge(local.x() * scale, local.y() * scale);

  return mapToScene(edge);
}

QPointF MindMapNode::parentAnchor(const QPointF &toward) const {
  return childAnchor(toward);
}

void MindMapNode::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
  Q_UNUSED(event);
  update();
}

void MindMapNode::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
  Q_UNUSED(event);
  update();
}