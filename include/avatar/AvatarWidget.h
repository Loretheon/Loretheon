#pragma once

#include <QQuickWidget>

// A QQuickWidget that hosts the 3D avatar scene. The scene is defined
// in resources/avatar/AvatarOverlay.qml, which instantiates the
// Balsam-generated RiggedFigure component.
class AvatarWidget : public QQuickWidget {
  Q_OBJECT

public:
  explicit AvatarWidget(QWidget *parent = nullptr);
  ~AvatarWidget() override;

  // Retained for API compatibility. The model is baked into the QML.
  void setModel(const QString &source);

  void setMouthOpen(float value);
  void setExpression(const QString &name);
  void playMotion(const QString &name);

  // Drive the mouth from a viseme shape. shape is one of "A", "E",
  // "I", "O", "U", "M", "sil". Call this as the viseme timeline
  // advances during speech.
  void applyViseme(const QString &shape);

  signals:
    void modelLoaded();
  void modelFailed(const QString &error);

private:
  QString m_modelSource;
};