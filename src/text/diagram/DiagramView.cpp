// DiagramView.cpp
#include "DiagramView.h"
#include "DiagramCanvas.h"
#include "DiagramDocument.h"

#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWheelEvent>

DiagramView::DiagramView(QWidget *parent) : QWidget(parent) {
  m_canvas = new DiagramCanvas(this);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidget(m_canvas);
  m_scroll->setWidgetResizable(false);
  m_scroll->setAlignment(Qt::AlignCenter);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setBackgroundRole(QPalette::Window);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->addWidget(m_scroll);

  setFocusPolicy(Qt::StrongFocus);
  m_scroll->viewport()->installEventFilter(this);

  connect(m_canvas, &DiagramCanvas::zoomChanged, this, &DiagramView::zoomChanged);
  connect(m_canvas, &DiagramCanvas::elementClicked, this, &DiagramView::elementClicked);
  connect(m_canvas, &DiagramCanvas::elementRightClicked, this, &DiagramView::elementRightClicked);
  connect(m_canvas, &DiagramCanvas::elementHovered, this, &DiagramView::elementHovered);
}

void DiagramView::setDocument(DiagramDocument *doc) {
  m_canvas->setDocument(doc);
}

DiagramDocument *DiagramView::document() const {
  return m_canvas->document();
}

qreal DiagramView::zoom() const {
  return m_canvas->zoom();
}

void DiagramView::zoomIn() {
  setZoom(m_canvas->zoom() * 1.25);
}

void DiagramView::zoomOut() {
  setZoom(m_canvas->zoom() / 1.25);
}

void DiagramView::zoomReset() {
  setZoom(1.0);
}

void DiagramView::zoomFit() {
  DiagramDocument *doc = m_canvas->document();
  if (!doc || doc->naturalSize().isEmpty()) return;
  const QSize svg = doc->naturalSize();
  const QSize vp  = m_scroll->viewport()->size();
  if (svg.isEmpty() || vp.isEmpty()) return;
  const qreal factor = qMin(qreal(vp.width())  / qreal(svg.width()),
                            qreal(vp.height()) / qreal(svg.height())) * 0.95;
  setZoom(factor);
}

void DiagramView::setZoom(qreal z) {
  m_canvas->setZoom(z);
}

void DiagramView::focusOnElement(const QString &id) {
  DiagramDocument *doc = m_canvas->document();
  if (!doc) return;
  const QRectF b = doc->boundsForId(id);
  if (b.isNull()) return;

  const QSize s = doc->naturalSize();
  if (s.isEmpty()) return;

  const qreal z = m_canvas->zoom();
  const QRectF box = QRectF(b.left() * z, b.top() * z,
                            b.width() * z, b.height() * z);

  const QSize vp = m_scroll->viewport()->size();
  m_scroll->horizontalScrollBar()->setValue(int(box.center().x() - vp.width()  / 2));
  m_scroll->verticalScrollBar()->setValue(int(box.center().y() - vp.height() / 2));
}

bool DiagramView::eventFilter(QObject *obj, QEvent *ev) {
  if (obj != m_scroll->viewport()) return QWidget::eventFilter(obj, ev);
  if (ev->type() != QEvent::Wheel) return QWidget::eventFilter(obj, ev);

  auto *we = static_cast<QWheelEvent *>(ev);
  if (we->modifiers() & Qt::ControlModifier) {
    const qreal oldZoom = m_canvas->zoom();
    const qreal step = we->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const qreal newZoom = qBound(0.05, oldZoom * step, 20.0);
    if (qFuzzyCompare(oldZoom, newZoom)) return true;

    const QPoint vpPos = we->position().toPoint();
    const QPoint canvasPos(
        int((m_scroll->horizontalScrollBar()->value() + vpPos.x()) * newZoom / oldZoom),
        int((m_scroll->verticalScrollBar()->value()   + vpPos.y()) * newZoom / oldZoom));

    setZoom(newZoom);

    m_scroll->horizontalScrollBar()->setValue(canvasPos.x() - vpPos.x());
    m_scroll->verticalScrollBar()->setValue(canvasPos.y() - vpPos.y());
    return true;
  }
  return QWidget::eventFilter(obj, ev);
}

void DiagramView::keyPressEvent(QKeyEvent *ev) {
  if (ev->key() == Qt::Key_Plus || ev->key() == Qt::Key_Equal) {
    zoomIn(); ev->accept(); return;
  }
  if (ev->key() == Qt::Key_Minus) {
    zoomOut(); ev->accept(); return;
  }
  if (ev->key() == Qt::Key_0 && (ev->modifiers() & Qt::ControlModifier)) {
    zoomReset(); ev->accept(); return;
  }
  if (ev->key() == Qt::Key_F && (ev->modifiers() & Qt::ControlModifier)) {
    zoomFit(); ev->accept(); return;
  }
  QWidget::keyPressEvent(ev);
}