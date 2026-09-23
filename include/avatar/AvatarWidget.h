#pragma once

#include <QPoint>
#include <QQuickWidget>

#include "AvatarConfig.h"
#include "AvatarResizeGrip.h"

class AvatarController;

// A QQuickWidget that hosts the 3D avatar scene. The scene is defined
// in resources/avatar/AvatarOverlay.qml. The CC Base model is loaded
// from QML via RuntimeLoader, and AvatarController binds ozz clips to
// the skeleton joints that the loader creates.
//
// The widget is free-floating: its geometry is owned by whoever
// constructs it, and it does not re-anchor itself when the main window
// resizes. Four corner grips are shown when resizable() is true. When
// the user drags a grip, the widget resizes about the opposite corner
// and emits geometryChanged().
//
// Dragging the body of the widget moves it, and turns her to face the
// direction she is being dragged.
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
  void onGripDragged(const QSize &newSize, AvatarResizeGrip::Corner corner);
  void onModelLoaded();

private:
  void pushConfigToQml();
  void pushFacingToQml();
  void layoutGrips();

  // Called once the QML model has finished loading. Loads the ozz
  // archives and binds them to the joint nodes the RuntimeLoader
  // created.
  void attachControllerToScene();

  QString m_modelSource;

  AvatarConfig m_config;

  AvatarResizeGrip *m_gripTopLeft = nullptr;
  AvatarResizeGrip *m_gripTopRight = nullptr;
  AvatarResizeGrip *m_gripBottomLeft = nullptr;
  AvatarResizeGrip *m_gripBottomRight = nullptr;

  bool m_resizable = false;
  bool m_configPending = false;

  AvatarController *m_controller = nullptr;
  bool m_archivesLoaded = false;
  bool m_controllerReady = false;

  // Body-drag state.
  bool m_dragging = false;
  QPoint m_dragOrigin;
  QPoint m_dragStartPosition;
  qreal m_facing = 0.0;
};