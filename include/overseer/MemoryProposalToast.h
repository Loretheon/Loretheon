#pragma once

#include <QWidget>

class QPropertyAnimation;
class QGraphicsOpacityEffect;
class QLabel;
class QTimer;

// A non-interactive notification card shown briefly when a new memory
// proposal arrives. It carries no buttons and accepts no clicks; all
// interaction happens in the "User actions needed" tab.
//
// Lifecycle:
//   - On construction, animates in (slide from the right + fade) over
//     SlideInMs milliseconds.
//   - Sits for m_lifetimeMs milliseconds.
//   - Fades out over FadeOutMs and emits finished(), after which the
//     owner should delete it.
//
// Calling dismiss() at any time skips the remaining lifetime and starts
// the fade-out immediately.
class MemoryProposalToast : public QWidget {
  Q_OBJECT

public:
  explicit MemoryProposalToast(const QString &fact,
                               const QString &rationale,
                               int lifetimeMs = 8000,
                               QWidget *parent = nullptr);

  // Abort the remaining lifetime and begin the fade-out.
  void dismiss();

  signals:
    void finished();

protected:
  void paintEvent(QPaintEvent *event) override;
  // Swallow all mouse events so the toast is purely visual.
  void mousePressEvent(QMouseEvent *event) override;
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
};