#include "../../include/avatar/AvatarWidget.h"

#include "../../include/avatar/AvatarController.h"
#include "../../include/avatar/AvatarMeshLoader.h"
#include "../../include/avatar/AvatarSurface.h"

#include <QDebug>
#include <QFile>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlContext>
#include <QResizeEvent>
#include <QTimer>
#include <QUrl>
#include <QSurfaceFormat>

namespace {

constexpr auto kAvatarMeshPath = ":/avatar/ccbase/Lore.glb";
constexpr auto kAvatarSkeletonPath = ":/avatar/ccbase/skeleton.ozz";
constexpr auto kAvatarClipDir = ":/avatar/ccbase";

constexpr int kCornerBand = 16;
constexpr int kEdgeBand = 8;

constexpr bool kInteractionEnabled = true;

} // namespace

AvatarWidget::AvatarWidget(QWidget *parent) : QQuickWidget(parent) {
  // setAttribute(Qt::WA_AlwaysStackOnTop, true);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_NoSystemBackground, true);

  setAutoFillBackground(false);

  {
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::transparent);
    setPalette(pal);
  }

  setClearColor(Qt::transparent);

  setResizeMode(QQuickWidget::SizeRootObjectToView);
  setMouseTracking(true);

  // X11-specific: Configure surface format for transparency
  QSurfaceFormat format;
  format.setAlphaBufferSize(8);
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  setFormat(format);

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

            pushFacingToQml();

            QQuickItem *root = rootObject();

            if (!root) {
              qWarning() << "[AvatarWidget] No QML root after Ready.";
              return;
            }

            root->setProperty("resizable", m_resizable);

            m_surface = root->findChild<QQuickItem *>(
                QStringLiteral("avatarSurface"),
                Qt::FindChildrenRecursively);

            if (!m_surface) {
              qWarning() << "[AvatarWidget] No AvatarSurface found in QML.";
              return;
            }

            connect(m_surface, SIGNAL(ready()), this,
                    SLOT(onSurfaceReady()), Qt::UniqueConnection);

            AvatarMeshData meshData;
            QString error;

            if (!AvatarMeshLoader::load(QString::fromLatin1(kAvatarMeshPath),
                                        meshData, &error)) {
              qWarning() << "[AvatarWidget] Mesh load failed:" << error;
              return;
            }

            auto *surface = qobject_cast<AvatarSurface *>(m_surface);

            if (!surface) {
              qWarning() << "[AvatarWidget] AvatarSurface cast failed.";
              return;
            }

            surface->setMeshData(meshData);
          });

  setSource(QUrl(QStringLiteral("qrc:/avatar/AvatarOverlay.qml")));
}

AvatarWidget::~AvatarWidget() = default;

void AvatarWidget::onSurfaceReady() {
  if (m_surfaceReady) {
    return;
  }

  m_surfaceReady = true;

  qDebug() << "[AvatarWidget] Surface ready, starting controller.";

  auto *surface = qobject_cast<AvatarSurface *>(m_surface);

  if (!surface) {
    qWarning() << "[AvatarWidget] Surface cast failed.";
    return;
  }

  m_controller->setSurface(surface);

  if (!m_controller->loadArchives(QString::fromLatin1(kAvatarSkeletonPath),
                                  QString::fromLatin1(kAvatarClipDir))) {
    qWarning() << "[AvatarWidget] Controller failed to load archives.";
    return;
  }

  m_controller->start();

  m_controller->playClip(QStringLiteral("idle"));

  emit modelLoaded();
}

void AvatarWidget::setSpeaking(bool speaking) {
  if (m_controller) {
    m_controller->setSpeaking(speaking);
  }
}

void AvatarWidget::applyConfig(const AvatarConfig &config) {
  m_config = config;
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

  QQuickItem *root = rootObject();

  if (root) {
    root->setProperty("resizable", m_resizable);
  }
}

AvatarWidget::DragKind AvatarWidget::bandFor(const QPoint &localPos) const {
  if (!m_resizable) {
    return DragKind::None;
  }

  const int w = width();
  const int h = height();

  const bool left = localPos.x() < kCornerBand;
  const bool right = localPos.x() > w - kCornerBand;
  const bool top = localPos.y() < kCornerBand;
  const bool bottom = localPos.y() > h - kCornerBand;

  if (left && top) {
    return DragKind::ResizeTopLeft;
  }
  if (right && top) {
    return DragKind::ResizeTopRight;
  }
  if (left && bottom) {
    return DragKind::ResizeBottomLeft;
  }
  if (right && bottom) {
    return DragKind::ResizeBottomRight;
  }

  const bool edgeLeft = localPos.x() < kEdgeBand;
  const bool edgeRight = localPos.x() > w - kEdgeBand;
  const bool edgeTop = localPos.y() < kEdgeBand;
  const bool edgeBottom = localPos.y() > h - kEdgeBand;

  if (edgeTop || edgeBottom || edgeLeft || edgeRight) {
    return DragKind::Move;
  }

  return DragKind::None;
}

void AvatarWidget::mousePressEvent(QMouseEvent *event) {
  if (!kInteractionEnabled) {
    QQuickWidget::mousePressEvent(event);
    return;
  }

  if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
    QQuickWidget::mousePressEvent(event);
    return;
  }

  const QPoint local = event->position().toPoint();
  const DragKind kind = bandFor(local);

  const bool isResize =
      kind == DragKind::ResizeTopLeft || kind == DragKind::ResizeTopRight ||
      kind == DragKind::ResizeBottomLeft ||
      kind == DragKind::ResizeBottomRight;

  const bool isMove = kind == DragKind::Move;

  if (isResize && event->button() != Qt::LeftButton) {
    QQuickWidget::mousePressEvent(event);
    return;
  }

  if (isMove && event->button() != Qt::RightButton) {
    QQuickWidget::mousePressEvent(event);
    return;
  }

  if (!isResize && !isMove) {
    QQuickWidget::mousePressEvent(event);
    return;
  }

  m_drag = kind;
  m_dragOriginGlobal = event->globalPosition().toPoint();
  m_originTopLeft = pos();
  m_originSize = size();

  event->accept();
}

void AvatarWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!kInteractionEnabled || m_drag == DragKind::None) {
    QQuickWidget::mouseMoveEvent(event);
    return;
  }

  const QPoint global = event->globalPosition().toPoint();
  const QPoint delta = global - m_dragOriginGlobal;

  if (m_drag == DragKind::Move) {
    move(m_originTopLeft + delta);
  } else {
    applyResize(delta);
  }

  event->accept();
}

void AvatarWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (!kInteractionEnabled || m_drag == DragKind::None) {
    QQuickWidget::mouseReleaseEvent(event);
    return;
  }

  m_drag = DragKind::None;
  emit geometryChanged();

  event->accept();
}

void AvatarWidget::applyResize(const QPoint &globalDelta) {
  int dx = globalDelta.x();
  int dy = globalDelta.y();

  switch (m_drag) {
  case DragKind::ResizeTopLeft:
    dx = -dx;
    dy = -dy;
    break;
  case DragKind::ResizeTopRight:
    dy = -dy;
    break;
  case DragKind::ResizeBottomLeft:
    dx = -dx;
    break;
  case DragKind::ResizeBottomRight:
    break;
  default:
    return;
  }

  const QSize minSize = m_config.minSize;
  const QSize maxSize = m_config.maxSize;

  const QSize raw(m_originSize.width() + dx,
                  m_originSize.height() + dy);

  const double aspect =
      (m_originSize.height() > 0)
          ? static_cast<double>(m_originSize.width()) / m_originSize.height()
          : 1.0;

  double w = raw.width();
  double h = raw.height();

  if (qAbs(raw.width() - m_originSize.width()) >=
      qAbs(raw.height() - m_originSize.height())) {
    h = w / aspect;
  } else {
    w = h * aspect;
  }

  if (w < minSize.width()) {
    w = minSize.width();
    h = w / aspect;
  }
  if (h < minSize.height()) {
    h = minSize.height();
    w = h * aspect;
  }
  if (w > maxSize.width()) {
    w = maxSize.width();
    h = w / aspect;
  }
  if (h > maxSize.height()) {
    h = maxSize.height();
    w = h * aspect;
  }

  const QSize newSize(qRound(w), qRound(h));

  QPoint newTopLeft = m_originTopLeft;

  switch (m_drag) {
  case DragKind::ResizeTopLeft:
    newTopLeft = m_originTopLeft +
                 QPoint(m_originSize.width() - newSize.width(),
                        m_originSize.height() - newSize.height());
    break;
  case DragKind::ResizeTopRight:
    newTopLeft = m_originTopLeft +
                 QPoint(0, m_originSize.height() - newSize.height());
    break;
  case DragKind::ResizeBottomLeft:
    newTopLeft = m_originTopLeft +
                 QPoint(m_originSize.width() - newSize.width(), 0);
    break;
  case DragKind::ResizeBottomRight:
    newTopLeft = m_originTopLeft;
    break;
  default:
    return;
  }

  setGeometry(QRect(newTopLeft, newSize));
}

void AvatarWidget::resizeEvent(QResizeEvent *event) {
  QQuickWidget::resizeEvent(event);
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
  }
}

void AvatarWidget::applyViseme(const QString &shape) {
  if (m_controller) {
    m_controller->applyViseme(shape);
  }
}

void AvatarWidget::applyVisemeBlend(const QString &from, const QString &to,
                                    float t) {
  if (m_controller) {
    m_controller->applyVisemeBlend(from, to, t);
  }
}