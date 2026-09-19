#include "../../include/overseer/ConductorDock.h"

#include "../../include/overseer/ConductorBoard.h"
#include "../../include/overseer/ConductorQueue.h"
#include "../../include/overseer/ConductorRoster.h"
#include "../../include/overseer/DependencyGraph.h"

#include "ThemeRegistry.h"
#include "ThemeTokens.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>

ConductorDock::ConductorDock(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("conductorDock"));
  setAttribute(Qt::WA_StyledBackground, true);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(16, 12, 16, 12);
  root->setSpacing(8);

  auto *headerRow = new QHBoxLayout;
  headerRow->setContentsMargins(0, 0, 0, 0);
  headerRow->setSpacing(8);

  auto *title = new QLabel(tr("Conductor"), this);
  QFont bold = title->font();
  bold.setBold(true);
  bold.setPointSize(bold.pointSize() + 1);
  title->setFont(bold);

  headerRow->addWidget(title);
  headerRow->addStretch(1);

  m_closeButton = new QPushButton(tr("Close"), this);
  connect(m_closeButton, &QPushButton::clicked, this, &ConductorDock::close);

  headerRow->addWidget(m_closeButton);

  root->addLayout(headerRow);

  m_board = new ConductorBoard(this);
  root->addWidget(m_board, 1);

  m_animation = new QPropertyAnimation(this, "geometry", this);
  m_animation->setDuration(kAnimationMs);
  m_animation->setEasingCurve(QEasingCurve::OutCubic);

  hide();

  if (parent)
    parent->installEventFilter(this);
}

void ConductorDock::setQueue(ConductorQueue *queue) {
  if (m_board)
    m_board->setQueue(queue);
}

void ConductorDock::setRoster(ConductorRoster *roster) {
  if (m_board)
    m_board->setRoster(roster);
}

void ConductorDock::setDependencies(DependencyGraph *graph) {
  if (m_board)
    m_board->setDependencies(graph);
}

void ConductorDock::setSessionFolder(const QString &folder) {
  if (m_board)
    m_board->setSessionFolder(folder);
}

void ConductorDock::setHeightFraction(double fraction) {
  m_heightFraction = qBound(0.2, fraction, 0.95);

  if (m_open)
    reposition();
}

bool ConductorDock::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() && event->type() == QEvent::Resize)
    reposition();

  return QWidget::eventFilter(watched, event);
}

void ConductorDock::open() {
  if (m_open)
    return;

  m_open = true;

  if (parentWidget()) {
    const int height =
        qMax(kMinimumHeight, int(parentWidget()->height() * m_heightFraction));

    setGeometry(0, -height, parentWidget()->width(), height);
    show();
    raise();

    animateTo(height);
  } else {
    show();
  }

  emit opened();
}

void ConductorDock::close() {
  if (!m_open)
    return;

  m_open = false;

  animateTo(0);

  emit closed();
}

void ConductorDock::toggle() {
  if (m_open)
    close();
  else
    open();
}

void ConductorDock::reposition() {
  if (!parentWidget())
    return;

  const QWidget *parent = parentWidget();

  if (m_open) {
    const int height =
        qMax(kMinimumHeight, int(parent->height() * m_heightFraction));

    setGeometry(0, 0, parent->width(), height);
  } else {
    const int height = qMax(kMinimumHeight, height);

    setGeometry(0, -height, parent->width(), height);
  }

  raise();
}

void ConductorDock::animateTo(int targetHeight) {
  if (!parentWidget())
    return;

  if (targetHeight <= 0) {
    disconnect(m_animation, nullptr, this, nullptr);

    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
      hide();
    });

    m_animation->stop();
    m_animation->setStartValue(geometry());
    m_animation->setEndValue(
        QRect(0, -height(), parentWidget()->width(), height()));
    m_animation->start();
    return;
  }

  disconnect(m_animation, nullptr, this, nullptr);

  m_animation->stop();
  m_animation->setStartValue(geometry());
  m_animation->setEndValue(
      QRect(0, 0, parentWidget()->width(), targetHeight));
  m_animation->start();
}

void ConductorDock::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const ThemeTokens tokens =
      ThemeRegistry::instance().tokens(ThemeRegistry::instance().activeTheme());

  QColor bg = tokens.base;
  bg.setAlpha(250);

  painter.setBrush(bg);

  QColor border = tokens.border;
  border.setAlpha(200);

  painter.setPen(QPen(border, 1));

  const QRectF r = rect().adjusted(0, 0, -1, -1);

  QPainterPath path;
  path.moveTo(r.topLeft());
  path.lineTo(r.topRight());
  path.lineTo(r.bottomRight() - QPointF(0, 8));
  path.quadTo(r.bottomRight(), r.bottomRight() - QPointF(8, 0));
  path.lineTo(r.bottomLeft() + QPointF(8, 0));
  path.quadTo(r.bottomLeft(), r.bottomLeft() - QPointF(0, 8));
  path.closeSubpath();

  painter.drawPath(path);

  painter.setPen(Qt::NoPen);

  QColor handle = tokens.textSubtle;
  handle.setAlpha(120);
  painter.setBrush(handle);

  const int handleWidth = 60;
  const int handleX = (width() - handleWidth) / 2;

  painter.drawRoundedRect(
      QRectF(handleX, height() - kHandleHeight + 2, handleWidth, 3), 1.5, 1.5);
}

void ConductorDock::mousePressEvent(QMouseEvent *event) {
  if (!event)
    return;

  const int y = event->position().toPoint().y();

  if (y >= height() - kHandleHeight) {
    m_resizing = true;
    m_resizeStartY = event->globalPosition().toPoint().y();
    m_resizeStartHeight = height();
    setCursor(Qt::SizeVerCursor);
    event->accept();
    return;
  }

  QWidget::mousePressEvent(event);
}

void ConductorDock::mouseMoveEvent(QMouseEvent *event) {
  if (!event)
    return;

  if (!m_resizing) {
    const int y = event->position().toPoint().y();

    if (y >= height() - kHandleHeight)
      setCursor(Qt::SizeVerCursor);
    else
      unsetCursor();

    QWidget::mouseMoveEvent(event);
    return;
  }

  const int delta =
      event->globalPosition().toPoint().y() - m_resizeStartY;

  const int newHeight = qMax(kMinimumHeight, m_resizeStartHeight + delta);

  if (parentWidget()) {
    const int maxHeight = int(parentWidget()->height() * 0.95);
    const int clamped = qMin(newHeight, maxHeight);

    setGeometry(0, 0, parentWidget()->width(), clamped);

    m_heightFraction =
        double(clamped) / double(qMax(1, parentWidget()->height()));
  }

  event->accept();
}

void ConductorDock::mouseReleaseEvent(QMouseEvent *event) {
  Q_UNUSED(event);

  if (m_resizing) {
    m_resizing = false;
    unsetCursor();
  }

  QWidget::mouseReleaseEvent(event);
}