// DiagramToolbar.h
#pragma once

#include <QWidget>

class QToolButton;

class DiagramToolbar : public QWidget {
  Q_OBJECT
public:
  explicit DiagramToolbar(QWidget *parent = nullptr);

  signals:
    void zoomInRequested();
  void zoomOutRequested();
  void zoomResetRequested();
  void fitRequested();
  void zoomToRequested(qreal factor);

public slots:
  void setZoom(qreal zoom);
  void setActionsEnabled(bool on);

private:
  QToolButton *makeButton(const QString &text, const QString &tip);
  void openPresetMenu();

  QToolButton *m_zoomOut   = nullptr;
  QToolButton *m_zoomIn    = nullptr;
  QToolButton *m_zoomLabel = nullptr;
  QToolButton *m_fit       = nullptr;
  QToolButton *m_reset     = nullptr;
};