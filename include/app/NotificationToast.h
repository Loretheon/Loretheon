#pragma once

#include "NotificationService.h"

#include <QPoint>
#include <QWidget>

class QLabel;
class QTimer;
class QGraphicsOpacityEffect;
class QPropertyAnimation;

// A single toast. Rendered by ToastStack on behalf of NotificationService.
// The toast is dismissible by click or by drag. It does not fire OS
// notifications itself; that is the service's job.
class NotificationToast : public QWidget {
  Q_OBJECT

public:
  explicit NotificationToast(const QString &id, const QString &title,
                             const QString &body,
                             NotificationService::Severity severity,
                             int lifetimeMs, QWidget *parent = nullptr);

  void dismiss();

  QString id() const { return m_id; }

  signals:
    void finished();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  void startFadeOut();
  void refreshPalette();
  QString severityColorRole() const;

  QString m_id;
  NotificationService::Severity m_severity =
      NotificationService::Severity::Info;

  QLabel *m_titleLabel = nullptr;
  QLabel *m_bodyLabel = nullptr;

  QTimer *m_lifetimeTimer = nullptr;
  QGraphicsOpacityEffect *m_opacity = nullptr;
  QPropertyAnimation *m_fade = nullptr;

  int m_lifetimeMs = 0;
  bool m_dismissed = false;
  bool m_hovered = false;

  bool m_dragging = false;
  QPoint m_dragStartGlobal;

  static constexpr int kDragDismissPx = 40;
  static constexpr int kToastRadius = 8;
  static constexpr int kFadeOutMs = 220;
};