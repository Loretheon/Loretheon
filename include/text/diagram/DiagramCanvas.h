// DiagramCanvas.h
#pragma once

#include <QWidget>
#include <QPointF>
#include <QSizeF>

class DiagramDocument;

class DiagramCanvas : public QWidget {
  Q_OBJECT
public:
  explicit DiagramCanvas(QWidget *parent = nullptr);

  void setDocument(DiagramDocument *doc);
  DiagramDocument *document() const { return m_doc; }

  void setZoom(qreal z);
  qreal zoom() const { return m_zoom; }

  QString selectedId() const { return m_selectedId; }
  void setSelectedId(const QString &id);

  signals:
    void elementClicked(const QString &id, const QString &name, const QPoint &globalPos);
  void elementRightClicked(const QString &id, const QString &name, const QPoint &globalPos);
  void elementHovered(const QString &id, const QString &name, const QPoint &globalPos);
  void backgroundClicked(const QPoint &globalPos);
  void zoomChanged(qreal zoom);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;

private slots:
  void onDocumentChanged();

private:
  QRectF letterboxRect() const;
  QPointF widgetToSvg(const QPointF &p) const;

  DiagramDocument *m_doc = nullptr;
  qreal m_zoom = 1.0;
  QString m_selectedId;
  QString m_hoveredId;
};