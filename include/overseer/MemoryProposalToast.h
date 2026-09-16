#pragma once

#include <QWidget>

class QLabel;
class QTimer;
class QGraphicsOpacityEffect;
class QPropertyAnimation;

// A non-interactive notification card shown briefly when a new memory
// proposal arrives. It carries no buttons and accepts no clicks; all
// interaction happens in the "User actions needed" tab.
//
// Lifecycle:
//   - On construction, sits in place and fades out after lifetimeMs.
//   - Calling dismiss() skips the remaining lifetime and begins fade-out.
//   - Emits finished() when fade-out completes; owner should delete it.
class MemoryProposalToast : public QWidget {
  Q_OBJECT

public:
  explicit MemoryProposalToast(const QString &fact,
                               const QString &rationale,
                               int lifetimeMs = 8000,
                               QWidget *parent = nullptr);

  void dismiss();

  signals:
    void finished();

protected:
  void paintEvent(QPaintEvent *event) override;
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