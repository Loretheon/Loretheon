#include "../../include/avatar/AvatarWidget.h"

#include "../../include/avatar/AvatarController.h"

#include <QDebug>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlContext>
#include <QResizeEvent>
#include <QUrl>

namespace {

// How many degrees she turns per pixel of horizontal drag.
constexpr qreal kDegreesPerPixel = 0.35;

// Clamp so she never turns past side-on.
constexpr qreal kMaxFacingDegrees = 80.0;

// Resource paths for the CC Base model and its ozz archives.
constexpr auto kAvatarResourceRoot = ":/avatar/ccbase";
constexpr auto kAvatarSkeletonPath = ":/avatar/ccbase/skeleton.ozz";

// RuntimeLoader::Success. The enum is Null=0, Success=1, Loading=2,
// Error=3 in Qt 6.4.
constexpr int kRuntimeLoaderSuccess = 1;

} // namespace

AvatarWidget::AvatarWidget(QWidget *parent) : QQuickWidget(parent) {
  setAttribute(Qt::WA_AlwaysStackOnTop, true);
  setClearColor(Qt::transparent);

  setResizeMode(QQuickWidget::SizeRootObjectToView);

  m_controller = new AvatarController(this);

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

            if (m_configPending) {
              m_configPending = false;
              pushConfigToQml();
            }

            pushFacingToQml();

            QQuickItem *root = rootObject();

            if (!root) {
              qWarning() << "[AvatarWidget] No QML root after Ready.";
              return;
            }

            // Load the ozz archives now. They do not depend on the
            // model load; they are independent of the scene graph.
            if (!m_archivesLoaded) {
              if (!m_controller->loadArchives(
                      QString::fromLatin1(kAvatarSkeletonPath),
                      QString::fromLatin1(kAvatarResourceRoot))) {
                qWarning() << "[AvatarWidget] Controller failed to load"
                              " ozz archives.";
                return;
              }

              m_archivesLoaded = true;
            }

            // The model load is asynchronous. Connect to the QML
            // signal and attach when it fires. If it already fired
            // before this connection was made, the status check
            // below catches that.
            connect(root, SIGNAL(modelLoaded()), this,
                    SLOT(onModelLoaded()), Qt::UniqueConnection);

            const QVariant loaderStatus = root->property("modelLoaderStatus");

            if (loaderStatus.isValid() &&
                loaderStatus.toInt() == kRuntimeLoaderSuccess) {
              attachControllerToScene();
        }
          });

  setSource(QUrl(QStringLiteral("qrc:/avatar/AvatarOverlay.qml")));

  m_gripTopLeft =
      new AvatarResizeGrip(AvatarResizeGrip::Corner::TopLeft, this);
  m_gripTopRight =
      new AvatarResizeGrip(AvatarResizeGrip::Corner::TopRight, this);
  m_gripBottomLeft =
      new AvatarResizeGrip(AvatarResizeGrip::Corner::BottomLeft, this);
  m_gripBottomRight =
      new AvatarResizeGrip(AvatarResizeGrip::Corner::BottomRight, this);

  for (AvatarResizeGrip *grip :
       {m_gripTopLeft, m_gripTopRight, m_gripBottomLeft, m_gripBottomRight}) {
    grip->hide();
    connect(grip, &AvatarResizeGrip::dragged, this,
            &AvatarWidget::onGripDragged);
    connect(grip, &AvatarResizeGrip::dragFinished, this,
            &AvatarWidget::geometryChanged);
  }

  layoutGrips();
}

AvatarWidget::~AvatarWidget() = default;

void AvatarWidget::onModelLoaded() {
  qDebug() << "[AvatarWidget] Model load signal received.";

  attachControllerToScene();
}

void AvatarWidget::attachControllerToScene() {
  if (m_controllerReady) {
    return;
  }

  if (!m_archivesLoaded) {
    qWarning() << "[AvatarWidget] attachControllerToScene: archives not"
                  " loaded yet. Deferring.";
    return;
  }

  QQuickItem *root = rootObject();

  if (!root) {
    qWarning() << "[AvatarWidget] attachControllerToScene: no QML root.";
    return;
  }

  if (!m_controller->attachToScene(root, QStringLiteral("modelRoot"))) {
    qWarning() << "[AvatarWidget] Controller failed to attach to the"
                  " scene graph.";
    return;
  }

  m_controller->playClip(QStringLiteral("idle"));
  m_controller->start();

  m_controllerReady = true;

  emit modelLoaded();

  qDebug() << "[AvatarWidget] Controller attached and playing idle.";
}

void AvatarWidget::applyConfig(const AvatarConfig &config) {
  m_config = config;

  for (AvatarResizeGrip *grip :
       {m_gripTopLeft, m_gripTopRight, m_gripBottomLeft, m_gripBottomRight}) {
    grip->setSizeBounds(config.minSize, config.maxSize);
  }

  if (!rootObject()) {
    m_configPending = true;
    return;
  }

  pushConfigToQml();
}

void AvatarWidget::pushConfigToQml() {
  QQuickItem *root = rootObject();

  if (!root) {
    return;
  }

  root->setProperty("cameraDistance", m_config.cameraDistance);
  root->setProperty("cameraHeight", m_config.cameraHeight);
  root->setProperty("cameraPitch", m_config.cameraPitch);
  root->setProperty("fieldOfView", m_config.fieldOfView);
  root->setProperty("figureOffsetY", m_config.figureOffsetY);
  root->setProperty("modelScale", m_config.modelScale);
}

void AvatarWidget::pushFacingToQml() {
  QQuickItem *root = rootObject();

  if (!root) {
    return;
  }

  root->setProperty("facing", m_facing);
}

void AvatarWidget::setResizable(bool resizable) {
  m_resizable = resizable;

  for (AvatarResizeGrip *grip :
       {m_gripTopLeft, m_gripTopRight, m_gripBottomLeft, m_gripBottomRight}) {
    grip->setVisible(resizable);
  }

  if (resizable) {
    layoutGrips();
    for (AvatarResizeGrip *grip :
         {m_gripTopLeft, m_gripTopRight, m_gripBottomLeft,
          m_gripBottomRight}) {
      grip->raise();
    }
  }
}

void AvatarWidget::resizeEvent(QResizeEvent *event) {
  QQuickWidget::resizeEvent(event);
  layoutGrips();
}

void AvatarWidget::layoutGrips() {
  const int g = AvatarResizeGrip::gripSize();

  if (m_gripTopLeft) {
    m_gripTopLeft->move(0, 0);
  }
  if (m_gripTopRight) {
    m_gripTopRight->move(width() - g, 0);
  }
  if (m_gripBottomLeft) {
    m_gripBottomLeft->move(0, height() - g);
  }
  if (m_gripBottomRight) {
    m_gripBottomRight->move(width() - g, height() - g);
  }
}

void AvatarWidget::onGripDragged(const QSize &newSize,
                                 AvatarResizeGrip::Corner corner) {
  QWidget *p = parentWidget();
  if (!p) {
    return;
  }

  QPoint anchor;

  switch (corner) {
  case AvatarResizeGrip::Corner::TopLeft:
    anchor = mapToParent(QPoint(width(), height()));
    break;
  case AvatarResizeGrip::Corner::TopRight:
    anchor = mapToParent(QPoint(0, height()));
    break;
  case AvatarResizeGrip::Corner::BottomLeft:
    anchor = mapToParent(QPoint(width(), 0));
    break;
  case AvatarResizeGrip::Corner::BottomRight:
    anchor = mapToParent(QPoint(0, 0));
    break;
  }

  QPoint newTopLeft = anchor;

  switch (corner) {
  case AvatarResizeGrip::Corner::TopLeft:
    newTopLeft = anchor - QPoint(newSize.width(), newSize.height());
    break;
  case AvatarResizeGrip::Corner::TopRight:
    newTopLeft = anchor - QPoint(0, newSize.height());
    break;
  case AvatarResizeGrip::Corner::BottomLeft:
    newTopLeft = anchor - QPoint(newSize.width(), 0);
    break;
  case AvatarResizeGrip::Corner::BottomRight:
    newTopLeft = anchor;
    break;
  }

  setGeometry(QRect(newTopLeft, newSize));
  layoutGrips();
}

void AvatarWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    event->ignore();
    return;
  }

  m_dragging = true;
  m_dragOrigin = event->globalPosition().toPoint();
  m_dragStartPosition = pos();

  event->accept();
}

void AvatarWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!m_dragging) {
    event->ignore();
    return;
  }

  const QPoint delta =
      event->globalPosition().toPoint() - m_dragOrigin;

  move(m_dragStartPosition + delta);

  if (delta.x() != 0) {
    m_facing = qBound(-kMaxFacingDegrees,
                      delta.x() * kDegreesPerPixel,
                      kMaxFacingDegrees);
    pushFacingToQml();
  }

  event->accept();
}

void AvatarWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton || !m_dragging) {
    event->ignore();
    return;
  }

  m_dragging = false;

  m_facing = 0.0;
  pushFacingToQml();

  emit geometryChanged();

  event->accept();
}

void AvatarWidget::placeByBottomRightOffset(const QPoint &offset) {
  QWidget *p = parentWidget();
  if (!p) {
    return;
  }

  const int x = p->width() - width() - offset.x();
  const int y = p->height() - height() - offset.y();

  move(x, y);
}

QPoint AvatarWidget::bottomRightOffset() const {
  QWidget *p = parentWidget();
  if (!p) {
    return {};
  }

  return QPoint(p->width() - (x() + width()),
                p->height() - (y() + height()));
}

void AvatarWidget::setModel(const QString &source) {
  m_modelSource = source;
}

void AvatarWidget::setMouthOpen(float value) {
  Q_UNUSED(value);
}

void AvatarWidget::setExpression(const QString &name) {
  Q_UNUSED(name);
}

void AvatarWidget::playMotion(const QString &name) {
  if (m_controller && m_controller->isLoaded()) {
    m_controller->playClip(name);
    return;
  }

  QQuickItem *root = rootObject();

  if (!root) {
    return;
  }

  QMetaObject::invokeMethod(root, "playMotion",
                            Q_ARG(QVariant, name));
}

void AvatarWidget::applyViseme(const QString &shape) {
  // The viseme path is still QML until the morph target driving moves
  // into C++. The QML stub keeps the log quiet. When morph targets
  // are wired, this routes through m_controller.
  QQuickItem *root = rootObject();

  if (!root) {
    return;
  }

  QMetaObject::invokeMethod(root, "setViseme",
                            Q_ARG(QVariant, shape));
}