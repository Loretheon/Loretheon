#include "../../include/overseer/AutoHideDock.h"

#include <QCursor>
#include <QEnterEvent>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr auto SettingsKeyWidthLeft = "overseer/dockWidthLeft";
constexpr auto SettingsKeyWidthRight = "overseer/dockWidthRight";
constexpr auto SettingsKeyExpandedLeft = "overseer/dockExpandedLeft";
constexpr auto SettingsKeyExpandedRight = "overseer/dockExpandedRight";

} // namespace

AutoHideDock::AutoHideDock(Edge edge, QWidget *parent)
    : QWidget(parent), m_edge(edge) {
  setObjectName(edge == Edge::Left ? QStringLiteral("autoHideDockLeft")
                                   : QStringLiteral("autoHideDockRight"));

  setAttribute(Qt::WA_StyledBackground, true);
  setMouseTracking(true);

  loadPersistedState();

  m_hideTimer = new QTimer(this);
  m_hideTimer->setSingleShot(true);
  m_hideTimer->setInterval(kHideDelayMs);

  connect(m_hideTimer, &QTimer::timeout, this, [this]() {
    if (m_pinned)
      return;

    if (mouseIsOverDock() || mouseIsOverStrip())
      return;

    hideDock();
  });

  // The strip lives inside the dock as a child. Its geometry is set
  // explicitly in updateStripGeometry() and it is raised above the
  // content when the dock is collapsed.
  m_strip = new QWidget(this);
  m_strip->setObjectName(
      m_edge == Edge::Left ? QStringLiteral("autoHideStripLeft")
                           : QStringLiteral("autoHideStripRight"));
  m_strip->setAttribute(Qt::WA_StyledBackground, true);
  m_strip->setMouseTracking(true);
  m_strip->installEventFilter(this);

  m_stripChevron = new QLabel(m_strip);
  m_stripChevron->setObjectName(QStringLiteral("autoHideStripChevron"));
  m_stripChevron->setAlignment(Qt::AlignCenter);
  m_stripChevron->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_stripChevron->setText(m_edge == Edge::Left ? QStringLiteral("\u203A")
                                               : QStringLiteral("\u2039"));

  auto *stripLayout = new QVBoxLayout(m_strip);
  stripLayout->setContentsMargins(0, 0, 0, 0);
  stripLayout->addWidget(m_stripChevron);

  updateStripGeometry();
  m_strip->setVisible(!m_expanded);
}

QString AutoHideDock::widthSettingsKey() const {
  return m_edge == Edge::Left ? QString::fromLatin1(SettingsKeyWidthLeft)
                              : QString::fromLatin1(SettingsKeyWidthRight);
}

QString AutoHideDock::expandedSettingsKey() const {
  return m_edge == Edge::Left ? QString::fromLatin1(SettingsKeyExpandedLeft)
                              : QString::fromLatin1(SettingsKeyExpandedRight);
}

void AutoHideDock::loadPersistedState() {
  QSettings settings;

  int stored = settings.value(widthSettingsKey(), 0).toInt();

  if (stored <= 0)
    stored = 280;

  m_dockWidth = qBound(kMinDockWidth, stored, kMaxDockWidth);

  m_expanded = settings.value(expandedSettingsKey(), true).toBool();
}

void AutoHideDock::persistWidth() {
  QSettings settings;
  settings.setValue(widthSettingsKey(), m_dockWidth);
}

void AutoHideDock::persistExpanded() {
  QSettings settings;
  settings.setValue(expandedSettingsKey(), m_expanded);
}

void AutoHideDock::setDockWidth(int width) {
  const int clamped = qBound(kMinDockWidth, width, kMaxDockWidth);

  if (m_dockWidth == clamped)
    return;

  m_dockWidth = clamped;

  persistWidth();

  emit dockWidthChanged(m_dockWidth);

  if (m_expanded)
    updateGeometry();
}

void AutoHideDock::setContent(QWidget *content) {
  if (m_content == content)
    return;

  if (m_content) {
    m_content->setParent(nullptr);
    m_content->deleteLater();
  }

  m_content = content;

  if (m_content) {
    m_content->setParent(this);
    m_content->setGeometry(rect());
    m_content->lower();
  }

  updateStripGeometry();
}

void AutoHideDock::setPinned(bool pinned) {
  if (m_pinned == pinned)
    return;

  m_pinned = pinned;

  emit pinnedChanged(pinned);

  if (m_pinned) {
    cancelHide();
    showDock();
  } else {
    if (!mouseIsOverDock() && !mouseIsOverStrip())
      scheduleHide();
  }
}

void AutoHideDock::togglePinned() { setPinned(!m_pinned); }

void AutoHideDock::showDock() {
  if (m_expanded)
    return;

  m_expanded = true;

  persistExpanded();

  updateGeometry();

  if (m_strip)
    m_strip->hide();

  emit expandedChanged(true);
}

void AutoHideDock::hideDock() {
  if (!m_expanded)
    return;

  if (m_pinned)
    return;

  m_expanded = false;

  persistExpanded();

  updateGeometry();

  if (m_strip) {
    updateStripGeometry();
    m_strip->show();
    m_strip->raise();
  }

  emit expandedChanged(false);
}

void AutoHideDock::scheduleHide() {
  if (m_pinned)
    return;

  m_hideTimer->start();
}

void AutoHideDock::cancelHide() { m_hideTimer->stop(); }

bool AutoHideDock::mouseIsOverDock() const {
  if (!isVisible())
    return false;

  const QPoint globalMouse = QCursor::pos();
  const QPoint localMouse = mapFromGlobal(globalMouse);

  return rect().contains(localMouse);
}

bool AutoHideDock::mouseIsOverStrip() const {
  if (!m_strip || !m_strip->isVisible())
    return false;

  const QPoint globalMouse = QCursor::pos();
  const QPoint localMouse = m_strip->mapFromGlobal(globalMouse);

  return m_strip->rect().contains(localMouse);
}

void AutoHideDock::enterEvent(QEnterEvent *event) {
  Q_UNUSED(event);
  cancelHide();
}

void AutoHideDock::leaveEvent(QEvent *event) {
  Q_UNUSED(event);

  if (m_pinned)
    return;

  scheduleHide();
}

void AutoHideDock::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  if (m_content)
    m_content->setGeometry(rect());

  updateStripGeometry();
}

void AutoHideDock::mouseMoveEvent(QMouseEvent *event) {
  if (!event)
    return;

  if (m_resizing) {
    const int globalX = event->globalPosition().toPoint().x();
    const int delta = globalX - m_resizeStartGlobalX;

    // Left dock grows when dragged right; right dock grows when
    // dragged left.
    const int newWidth = (m_edge == Edge::Left)
                             ? m_resizeStartWidth + delta
                             : m_resizeStartWidth - delta;

    setDockWidth(newWidth);
    return;
  }

  updateCursor(event->position().toPoint());
}

void AutoHideDock::mousePressEvent(QMouseEvent *event) {
  if (!event)
    return;

  if (event->button() != Qt::LeftButton)
    return;

  if (!m_expanded)
    return;

  const QPoint pos = event->position().toPoint();

  const bool onHandle = (m_edge == Edge::Left)
                            ? pos.x() >= width() - kResizeHandleWidth
                            : pos.x() <= kResizeHandleWidth;

  if (!onHandle)
    return;

  m_resizing = true;
  m_resizeStartWidth = m_dockWidth;
  m_resizeStartGlobalX = event->globalPosition().toPoint().x();
}

void AutoHideDock::mouseReleaseEvent(QMouseEvent *event) {
  Q_UNUSED(event);

  if (m_resizing) {
    m_resizing = false;
    persistWidth();
    updateCursor(mapFromGlobal(QCursor::pos()));
  }
}

void AutoHideDock::updateCursor(const QPoint &pos) {
  if (!m_expanded) {
    unsetCursor();
    return;
  }

  const bool onHandle = (m_edge == Edge::Left)
                            ? pos.x() >= width() - kResizeHandleWidth
                            : pos.x() <= kResizeHandleWidth;

  if (onHandle) {
    setCursor(Qt::SizeHorCursor);
  } else {
    unsetCursor();
  }
}

bool AutoHideDock::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_strip) {
    if (event->type() == QEvent::Enter) {
      cancelHide();
      showDock();
      return false;
    }

    if (event->type() == QEvent::Leave) {
      if (!m_pinned && !mouseIsOverDock())
        scheduleHide();
      return false;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void AutoHideDock::updateStripGeometry() {
  if (!m_strip)
    return;

  // The strip is anchored to the outer edge of the dock (left edge for
  // a Left dock, right edge for a Right dock). Its width is the strip
  // width regardless of the dock's current width.
  const int w = kStripWidth;
  const int h = height();

  const int x = (m_edge == Edge::Left) ? 0 : qMax(0, width() - w);

  m_strip->setGeometry(x, 0, w, h);

  if (m_stripChevron)
    m_stripChevron->setGeometry(0, 0, w, h);
}