#pragma once

#include <QPoint>
#include <QQuickWidget>
#include <QSize>

#include "AvatarConfig.h"

class AvatarController;
class AvatarSurface;

class AvatarWidget : public QQuickWidget {
  Q_OBJECT

public:
  explicit AvatarWidget(QWidget *parent = nullptr);
  ~AvatarWidget() override;
  void setSpeaking(bool speaking);
  void applyConfig(const AvatarConfig &config);

  void setResizable(bool resizable);
  bool isResizable() const { return m_resizable; }

  QSize defaultSize() const { return m_config.widgetSize; }

  void setModel(const QString &source);

  void setMouthOpen(float value);
  void setExpression(const QString &name);
  void playMotion(const QString &name);
  void applyViseme(const QString &shape);
  void applyVisemeBlend(const QString &from, const QString &to, float t);
  void placeByBottomRightOffset(const QPoint &offset);
  QPoint bottomRightOffset() const;

  signals:
    void geometryChanged();
  void modelLoaded();
  void modelFailed(const QString &error);

protected:
  void resizeEvent(QResizeEvent *event) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
  void onSurfaceReady();

private:
  enum class DragKind {
    None,
    Move,
    ResizeTopLeft,
    ResizeTopRight,
    ResizeBottomLeft,
    ResizeBottomRight,
  };

  void pushFacingToQml();

  DragKind bandFor(const QPoint &localPos) const;
  void applyResize(const QPoint &globalDelta);

  QString m_modelSource;

  AvatarConfig m_config;

  bool m_resizable = false;

  DragKind m_drag = DragKind::None;
  QPoint m_dragOriginGlobal;
  QPoint m_originTopLeft;
  QSize m_originSize;

  AvatarController *m_controller = nullptr;
  QQuickItem *m_surface = nullptr;

  bool m_surfaceReady = false;

  qreal m_facing = 0.0;
};