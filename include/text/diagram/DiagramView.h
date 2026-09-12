// DiagramView.h
#pragma once

#include <QWidget>

class DiagramCanvas;
class DiagramDocument;
class QScrollArea;

class DiagramView : public QWidget {
  Q_OBJECT
public:
  explicit DiagramView(QWidget *parent = nullptr);

  void setDocument(DiagramDocument *doc);
  DiagramDocument *document() const;

  qreal zoom() const;

public slots:
  void zoomIn();
  void zoomOut();
  void zoomReset();
  void zoomFit();
  void setZoom(qreal z);
  void focusOnElement(const QString &id);

  signals:
    void zoomChanged(qreal zoom);
  void elementClicked(const QString &id, const QString &name, const QPoint &globalPos);
  void elementRightClicked(const QString &id, const QString &name, const QPoint &globalPos);
  void elementHovered(const QString &id, const QString &name, const QPoint &globalPos);

protected:
  bool eventFilter(QObject *obj, QEvent *ev) override;
  void keyPressEvent(QKeyEvent *ev) override;

private:
  DiagramCanvas *m_canvas = nullptr;
  QScrollArea   *m_scroll = nullptr;
};