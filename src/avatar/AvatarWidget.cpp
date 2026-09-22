#include "../../include/avatar/AvatarWidget.h"

#include <QDebug>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlContext>
#include <QUrl>

AvatarWidget::AvatarWidget(QWidget *parent) : QQuickWidget(parent) {
  setAttribute(Qt::WA_AlwaysStackOnTop, true);
  setClearColor(Qt::transparent);

  setResizeMode(QQuickWidget::SizeRootObjectToView);

  connect(this, &QQuickWidget::statusChanged, this,
          [this](QQuickWidget::Status status) {
            if (status != QQuickWidget::Ready) {
              if (status == QQuickWidget::Error) {
                qWarning() << "[AvatarWidget] QML load failed:";
                for (const QQmlError &error : errors()) {
                  qWarning() << "  " << error.toString();
                }
              }
              return;
            }

            qDebug() << "[AvatarWidget] QML ready";
          });

  setSource(QUrl(QStringLiteral("qrc:/avatar/AvatarOverlay.qml")));
}

AvatarWidget::~AvatarWidget() = default;

void AvatarWidget::setModel(const QString &source) {
  // The model is baked into the QML scene. Setting a source is
  // retained for API compatibility but does not change the scene.
  m_modelSource = source;
}

void AvatarWidget::setMouthOpen(float value) {
  QQuickItem *root = rootObject();

  if (!root) {
    return;
  }

  root->setProperty("mouthA", value);
}

void AvatarWidget::setExpression(const QString &name) {
  QQuickItem *root = rootObject();

  if (!root) {
    return;
  }

  root->setProperty("expression", name);
}

void AvatarWidget::playMotion(const QString &name) {
  QQuickItem *root = rootObject();

  if (!root) {
    return;
  }

  QMetaObject::invokeMethod(root, "playMotion",
                            Q_ARG(QVariant, name));
}

void AvatarWidget::applyViseme(const QString &shape) {
  QQuickItem *root = rootObject();

  if (!root) {
    return;
  }

  QMetaObject::invokeMethod(root, "setViseme",
                            Q_ARG(QVariant, shape));
}