#pragma once

#include <QPoint>
#include <QWidget>

class QLabel;
class QTimer;
class QGraphicsOpacityEffect;
class QPropertyAnimation;

// A notification card shown briefly when a new memory proposal arrives.
// The card can be dismissed by clicking it, or dragged aside. It carries
// no buttons; all durable interaction happens in the proposal card in
// the transcript.
class MemoryProposalToast : public QWidget {
  Q_OBJECT

public:
  explicit MemoryProposalToast(const QString &fact,
                               const QString &rationale,
                               const QString &scopeLabel,
                               int lifetimeMs = 8000,
                               QWidget *parent = nullptr);

  void dismiss();

  signals:
    void finished();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  void startFadeOut();

  QLabel *m_factLabel = nullptr;
  QLabel *m_rationaleLabel = nullptr;

  QTimer *m_lifetimeTimer = nullptr;
  QGraphicsOpacityEffect *m_opacity = nullptr;
  QPropertyAnimation *m_fade = nullptr;

  int m_lifetimeMs = 8000;
  bool m_dismissed = false;

  bool m_dragging = false;
  QPoint m_dragStartGlobal;

  static constexpr int kDragDismissPx = 40;
};