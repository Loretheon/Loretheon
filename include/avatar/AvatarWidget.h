#pragma once

#include <QPoint>
#include <QQuickWidget>
#include <QSize>

#include "AvatarConfig.h"

class AvatarController;
class AvatarSurface;

// A QQuickWidget that hosts the 3D avatar scene.
//
// Grip drags are handled here, in the widget's own mouse events, not
// in QML. The QML grips draw the affordance and set the cursor; they
// do not drive the drag. Driving a drag from QML fails because the
// grip item moves while the widget resizes, which moves the local
// mouse coordinate out from under the handler and produces a runaway
// resize. QMouseEvent::globalPosition is stable across a resize.
//
// Presses in the interior are forwarded to the QML scene, which
// orbits, pans, and zooms.
class AvatarWidget : public QQuickWidget {
  Q_OBJECT

public:
  explicit AvatarWidget(QWidget *parent = nullptr);
  ~AvatarWidget() override;

  void applyConfig(const AvatarConfig &config);

  void setResizable(bool resizable);
  bool isResizable() const { return m_resizable; }

  QSize defaultSize() const { return m_config.widgetSize; }

  void setModel(const QString &source);

  void setMouthOpen(float value);
  void setExpression(const QString &name);
  void playMotion(const QString &name);
  void applyViseme(const QString &shape);

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

  // Which band the widget-local point is in.
  DragKind bandFor(const QPoint &localPos) const;

  // Apply a resize given the accumulated global delta from press.
  void applyResize(const QPoint &globalDelta);

  QString m_modelSource;

  AvatarConfig m_config;

  bool m_resizable = false;

  // Active drag state. All positions are global (screen) coordinates.
  DragKind m_drag = DragKind::None;
  QPoint m_dragOriginGlobal;
  QPoint m_originTopLeft;
  QSize m_originSize;

  AvatarController *m_controller = nullptr;
  QQuickItem *m_surface = nullptr;

  bool m_surfaceReady = false;

  qreal m_facing = 0.0;
};