#include "DiagramCanvas.h"
#include "DiagramDocument.h"

#include <QMouseEvent>
#include <QPainter>
#include <QSvgRenderer>
#include <QToolTip>

DiagramCanvas::DiagramCanvas(QWidget *parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setMinimumSize(1, 1);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setAutoFillBackground(false);
}

void DiagramCanvas::setDocument(DiagramDocument *doc) {
  if (m_doc == doc) return;
  if (m_doc) disconnect(m_doc, nullptr, this, nullptr);
  m_doc = doc;
  if (m_doc) {
    connect(m_doc, &DiagramDocument::changed, this, &DiagramCanvas::onDocumentChanged);
  }
  onDocumentChanged();
}

void DiagramCanvas::onDocumentChanged() {
  m_selectedId.clear();
  m_hoveredId.clear();
  const QSize s = m_doc ? m_doc->naturalSize() : QSize();
  if (s.isEmpty()) setFixedSize(1, 1);
  else setFixedSize(qMax(1, int(s.width()  * m_zoom)),
                    qMax(1, int(s.height() * m_zoom)));
  unsetCursor();
  update();
}

void DiagramCanvas::setZoom(qreal z) {
  z = qBound(0.05, z, 20.0);
  if (qFuzzyCompare(z, m_zoom)) return;
  m_zoom = z;
  onDocumentChanged();
  emit zoomChanged(m_zoom);
}

void DiagramCanvas::setSelectedId(const QString &id) {
  if (m_selectedId == id) return;
  m_selectedId = id;
  update();
}

QRectF DiagramCanvas::letterboxRect() const {
  if (!m_doc || m_doc->naturalSize().isEmpty()) return rect();
  const QSize s = m_doc->naturalSize();
  const qreal svgAspect = qreal(s.width()) / qreal(s.height());
  const qreal boxAspect = qreal(width())     / qreal(height());

  QSizeF target;
  if (boxAspect > svgAspect) {
    target.setHeight(height());
    target.setWidth(height() * svgAspect);
  } else {
    target.setWidth(width());
    target.setHeight(width() / svgAspect);
  }
  return QRectF((width()  - target.width())  / 2.0,
                (height() - target.height()) / 2.0,
                target.width(), target.height());
}

QPointF DiagramCanvas::widgetToSvg(const QPointF &p) const {
  if (!m_doc || m_doc->naturalSize().isEmpty()) return {};
  const QRectF box = letterboxRect();
  const QSize s = m_doc->naturalSize();
  const qreal sx = box.width()  / qreal(s.width());
  const qreal sy = box.height() / qreal(s.height());
  return QPointF((p.x() - box.left()) / sx,
                 (p.y() - box.top())  / sy);
}

QRectF DiagramCanvas::svgRectToWidget(const QRectF &svgRect) const {
  if (!m_doc || m_doc->naturalSize().isEmpty()) return {};
  const QRectF box = letterboxRect();
  const QSize s = m_doc->naturalSize();
  const qreal sx = box.width()  / qreal(s.width());
  const qreal sy = box.height() / qreal(s.height());
  return QRectF(box.left() + svgRect.left()   * sx,
                box.top()  + svgRect.top()    * sy,
                svgRect.width()  * sx,
                svgRect.height() * sy);
}

void DiagramCanvas::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.fillRect(rect(), palette().color(QPalette::Window));

  if (!m_doc || !m_doc->renderer()->isValid()) return;

  const QRectF box = letterboxRect();
  m_doc->renderer()->render(&p, box);

  const QColor accent("#8caaee");

  if (!m_hoveredId.isEmpty()) {
    const QRectF b = m_doc->boundsForId(m_hoveredId);
    if (!b.isNull()) {
      QRectF r = svgRectToWidget(b).adjusted(-2, -2, 2, 2);

      QColor wash = accent;
      wash.setAlpha(40);
      p.setPen(Qt::NoPen);
      p.setBrush(wash);
      p.drawRoundedRect(r, 5, 5);

      QColor line = accent;
      line.setAlpha(160);
      QPen pen(line);
      pen.setWidthF(1.5);
      p.setPen(pen);
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(r, 5, 5);
    }
  }

  if (!m_selectedId.isEmpty()) {
    const QRectF b = m_doc->boundsForId(m_selectedId);
    if (!b.isNull()) {
      QRectF r = svgRectToWidget(b).adjusted(-2, -2, 2, 2);

      QColor wash = accent;
      wash.setAlpha(55);
      p.setPen(Qt::NoPen);
      p.setBrush(wash);
      p.drawRoundedRect(r, 5, 5);

      QPen pen(accent);
      pen.setWidthF(2.0);
      p.setPen(pen);
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(r, 5, 5);
    }
  }
}

void DiagramCanvas::updateCursorForHover(const QString &id) {
  if (id.isEmpty() || !m_doc) { unsetCursor(); return; }
  const auto info = m_doc->infoForId(id);
  if (info.nodeKind == DiagramDocument::NodeKind::Reference ||
      info.nodeKind == DiagramDocument::NodeKind::External)
    setCursor(Qt::PointingHandCursor);
  else
    unsetCursor();
}

void DiagramCanvas::mousePressEvent(QMouseEvent *ev) {
  if (!m_doc || !m_doc->renderer()->isValid()) return;
  const QPointF svgPt = widgetToSvg(ev->position());
  const QString id = m_doc->idAt(svgPt);
  const QString name = id.isEmpty() ? QString() : m_doc->nameForId(id);

  if (ev->button() == Qt::RightButton) {
    if (!id.isEmpty()) {
      setSelectedId(id);
      emit elementRightClicked(id, name, ev->globalPosition().toPoint());
    } else {
      emit backgroundClicked(ev->globalPosition().toPoint());
    }
  } else if (ev->button() == Qt::LeftButton) {
    if (!id.isEmpty()) {
      setSelectedId(id);
      emit elementClicked(id, name, ev->globalPosition().toPoint());

      const auto info = m_doc->infoForId(id);
      if (info.nodeKind != DiagramDocument::NodeKind::Plain &&
          !info.referencePath.isEmpty()) {
        emit elementActivated(id, info.referencePath, info.nodeKind);
      }
    } else {
      setSelectedId(QString());
      emit backgroundClicked(ev->globalPosition().toPoint());
    }
  }
}

void DiagramCanvas::mouseDoubleClickEvent(QMouseEvent *ev) {
  if (!m_doc || ev->button() != Qt::LeftButton) return;
  const QPointF svgPt = widgetToSvg(ev->position());
  const QString id = m_doc->idAt(svgPt);
  if (id.isEmpty()) return;
  const auto info = m_doc->infoForId(id);
  if (info.nodeKind != DiagramDocument::NodeKind::Plain &&
      !info.referencePath.isEmpty()) {
    emit elementActivated(id, info.referencePath, info.nodeKind);
  }
}

void DiagramCanvas::mouseMoveEvent(QMouseEvent *ev) {
  if (!m_doc || !m_doc->renderer()->isValid()) return;
  const QPointF svgPt = widgetToSvg(ev->position());
  const QString id = m_doc->idAt(svgPt);

  if (id != m_hoveredId) {
    m_hoveredId = id;
    updateCursorForHover(id);

    if (!id.isEmpty()) {
      const auto info = m_doc->infoForId(id);
      QString tip = info.name;
      if (info.nodeKind == DiagramDocument::NodeKind::External) {
        tip = tr("Open externally: %1").arg(info.referencePath);
      } else if (info.nodeKind == DiagramDocument::NodeKind::Reference) {
        tip = tr("Open: %1").arg(info.referencePath);
      }
      if (!tip.isEmpty()) QToolTip::showText(ev->globalPosition().toPoint(), tip, this);
      else                QToolTip::hideText();
    } else {
      QToolTip::hideText();
    }

    emit elementHovered(id,
                        id.isEmpty() ? QString() : m_doc->nameForId(id),
                        ev->globalPosition().toPoint());
    update();
  }
}

void DiagramCanvas::leaveEvent(QEvent *) {
  if (!m_hoveredId.isEmpty()) {
    m_hoveredId.clear();
    unsetCursor();
    QToolTip::hideText();
    emit elementHovered(QString(), QString(), QPoint());
    update();
  }
}