#include "AutoHideDock.h"

#include <QCursor>
#include <QEnterEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

AutoHideDock::AutoHideDock(Edge edge, const QString &settingsPrefix,
                           QWidget *parent)
    : QWidget(parent), m_edge(edge), m_settingsPrefix(settingsPrefix) {
  setObjectName(edge == Edge::Left ? QStringLiteral("autoHideDockLeft")
                                   : QStringLiteral("autoHideDockRight"));

  setAttribute(Qt::WA_StyledBackground, true);
  setMouseTracking(true);

  loadPersistedState();

  m_hideTimer = new QTimer(this);
  m_hideTimer->setSingleShot(true);
  m_hideTimer->setInterval(kDefaultHideDelayMs);

  connect(m_hideTimer, &QTimer::timeout, this, [this]() {
    if (m_pinned)
      return;

    if (mouseIsOverDock() || mouseIsOverStrip())
      return;

    // If the user is currently dragging the handle, do not collapse.
    if (m_resizing)
      return;

    hideDock();
  });

  // The strip is only shown when the dock is collapsed. When the dock
  // is expanded, the drag handle is drawn directly on the dock body.
  m_strip = new QWidget(this);
  m_strip->setObjectName(
      m_edge == Edge::Left ? QStringLiteral("autoHideStripLeft")
                           : QStringLiteral("autoHideStripRight"));
  m_strip->setAttribute(Qt::WA_StyledBackground, true);
  m_strip->setMouseTracking(true);
  m_strip->installEventFilter(this);

  m_stripChevron = new QWidget(m_strip);
  m_stripChevron->setObjectName(QStringLiteral("autoHideStripGrip"));
  m_stripChevron->setAttribute(Qt::WA_StyledBackground, true);
  m_stripChevron->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_stripChevron->setFixedSize(4, 28);

  auto *stripLayout = new QVBoxLayout(m_strip);
  stripLayout->setContentsMargins(0, 0, 0, 0);
  stripLayout->setSpacing(0);
  stripLayout->addStretch(1);
  stripLayout->addWidget(m_stripChevron, 0, Qt::AlignHCenter);
  stripLayout->addStretch(1);

  updateStripGeometry();
  m_strip->setVisible(!m_expanded);
}

QString AutoHideDock::widthSettingsKey() const {
  return m_settingsPrefix +
         (m_edge == Edge::Left ? QStringLiteral("/widthLeft")
                               : QStringLiteral("/widthRight"));
}

QString AutoHideDock::overrideSettingsKey() const {
  return m_settingsPrefix +
         (m_edge == Edge::Left ? QStringLiteral("/widthOverrideLeft")
                               : QStringLiteral("/widthOverrideRight"));
}

QString AutoHideDock::expandedSettingsKey() const {
  return m_settingsPrefix +
         (m_edge == Edge::Left ? QStringLiteral("/expandedLeft")
                               : QStringLiteral("/expandedRight"));
}

void AutoHideDock::loadPersistedState() {
  QSettings settings;

  int stored = settings.value(widthSettingsKey(), 0).toInt();

  if (stored <= 0)
    stored = 280;

  m_dockWidth = std::clamp(stored, MinDockWidth, m_maxDockWidth);

  m_userOverrideWidth =
      settings.value(overrideSettingsKey(), false).toBool();

  m_expanded = settings.value(expandedSettingsKey(), true).toBool();
}

void AutoHideDock::persistWidth() {
  QSettings settings;
  settings.setValue(widthSettingsKey(), m_dockWidth);
}

void AutoHideDock::persistOverride() {
  QSettings settings;
  settings.setValue(overrideSettingsKey(), m_userOverrideWidth);
}

void AutoHideDock::persistExpanded() {
  QSettings settings;
  settings.setValue(expandedSettingsKey(), m_expanded);
}

void AutoHideDock::setMaxDockWidth(int width) {
  int clamped = std::max(MinDockWidth, width);

  // Once the natural width is established it is also the ceiling.
  // No caller may widen the dock past what the content asked for.
  if (m_naturalWidth > 0)
    clamped = std::min(clamped, m_naturalWidth);

  if (m_maxDockWidth == clamped)
    return;

  m_maxDockWidth = clamped;

  if (m_dockWidth > m_maxDockWidth)
    setDockWidth(m_maxDockWidth);
}

void AutoHideDock::setDockWidth(int width) {
  const int clamped = std::clamp(width, MinDockWidth, m_maxDockWidth);

  if (m_dockWidth == clamped)
    return;

  m_dockWidth = clamped;

  persistWidth();

  emit dockWidthChanged(m_dockWidth);

  if (m_expanded)
    updateGeometry();
}

void AutoHideDock::setPreferredContentWidth(int width) {
  if (width <= 0) {
    m_preferredContentWidth = 0;
    return;
  }

  m_preferredContentWidth = width;
}

void AutoHideDock::fitToContentWidth() {
  if (m_userOverrideWidth)
    return;

  if (m_preferredContentWidth <= 0)
    return;

  // Re-derive the natural width from the current content
  // measurement. The tree's reported width is not monotonic (it
  // grows when a subtree is expanded and shrinks when the widest
  // entry is removed or renamed), so a one-shot latch pins the dock
  // to whichever measurement happened to arrive first — usually the
  // fully-expanded one from expandAllAndMeasure().
  m_naturalWidth = m_preferredContentWidth;

  setMaxDockWidth(m_naturalWidth);
  setDockWidth(m_naturalWidth);
}

void AutoHideDock::clearUserOverrideWidth() {
  if (!m_userOverrideWidth)
    return;

  m_userOverrideWidth = false;

  // Forget the latched natural width so the next fit re-derives it
  // from the current content measurement instead of re-applying a
  // stale value.
  m_naturalWidth = 0;

  persistOverride();

  fitToContentWidth();
}

void AutoHideDock::setContent(QWidget *content) {
  if (m_content == content)
    return;

  if (m_content) {
    m_content->removeEventFilter(this);
    m_content->setParent(nullptr);
    m_content->deleteLater();
  }

  m_content = content;

  if (m_content) {
    m_content->setParent(this);
    m_content->installEventFilter(this);

    updateContentGeometry();
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

void AutoHideDock::showDockImmediate() {
  if (m_expanded)
    return;

  m_expanded = true;

  persistExpanded();

  updateGeometry();

  if (m_strip)
    m_strip->hide();

  emit expandedChanged(true);
}

void AutoHideDock::hideDockImmediate() {
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

void AutoHideDock::setHideDelayMs(int ms) {
  m_hideTimer->setInterval(std::max(0, ms));
}

int AutoHideDock::hideDelayMs() const {
  return m_hideTimer->interval();
}

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

bool AutoHideDock::mouseIsOverResizeHandle(const QPoint &localPos) const {
  if (!m_expanded)
    return false;

  if (m_edge == Edge::Left) {
    return localPos.x() >= width() - ResizeHandleWidth;
  }
  return localPos.x() <= ResizeHandleWidth;
}

void AutoHideDock::enterEvent(QEnterEvent *event) {
  Q_UNUSED(event);
  cancelHide();
}

void AutoHideDock::leaveEvent(QEvent *event) {
  Q_UNUSED(event);

  if (m_pinned)
    return;

  m_hoveringHandle = false;
  update();

  scheduleHide();
}

void AutoHideDock::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  updateContentGeometry();
  updateStripGeometry();
}

void AutoHideDock::updateContentGeometry() {
  if (!m_content)
    return;

  const QRect r = rect();

  if (m_edge == Edge::Left) {
    m_content->setGeometry(r.left(), r.top(),
                           std::max(0, r.width() - ResizeHandleWidth),
                           r.height());
  } else {
    m_content->setGeometry(r.left() + ResizeHandleWidth, r.top(),
                           std::max(0, r.width() - ResizeHandleWidth),
                           r.height());
  }
}

void AutoHideDock::mouseMoveEvent(QMouseEvent *event) {
  if (!event)
    return;

  if (m_resizing) {
    const int globalX = event->globalPosition().toPoint().x();
    const int delta = globalX - m_resizeStartGlobalX;

    const int newWidth = (m_edge == Edge::Left)
                             ? m_resizeStartWidth + delta
                             : m_resizeStartWidth - delta;

    if (!m_userOverrideWidth) {
      m_userOverrideWidth = true;
      persistOverride();
    }

    setDockWidth(newWidth);
    return;
  }

  const QPoint pos = event->position().toPoint();

  const bool wasHovering = m_hoveringHandle;
  m_hoveringHandle = mouseIsOverResizeHandle(pos);

  if (wasHovering != m_hoveringHandle)
    update();

  updateCursor(pos);
}

void AutoHideDock::mousePressEvent(QMouseEvent *event) {
  if (!event)
    return;

  if (event->button() != Qt::LeftButton)
    return;

  if (!m_expanded)
    return;

  const QPoint pos = event->position().toPoint();

  if (!mouseIsOverResizeHandle(pos))
    return;

  cancelHide();

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

void AutoHideDock::paintEvent(QPaintEvent *event) {
  QWidget::paintEvent(event);

  if (!m_expanded)
    return;

  // Draw the resize affordance directly on the dock body. The strip
  // widget is only present when the dock is collapsed, so we need a
  // separate visual cue for the expanded state.
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const int barWidth = 2;
  const int barHeight = 28;

  const int x = (m_edge == Edge::Left)
                    ? width() - (ResizeHandleWidth / 2) - (barWidth / 2)
                    : (ResizeHandleWidth / 2) - (barWidth / 2);

  const int y = (height() - barHeight) / 2;

  QColor colour = palette().color(QPalette::Mid);
  colour.setAlpha(m_hoveringHandle ? 200 : 90);

  painter.setPen(Qt::NoPen);
  painter.setBrush(colour);
  painter.drawRoundedRect(QRect(x, y, barWidth, barHeight), 1, 1);
}

void AutoHideDock::updateCursor(const QPoint &pos) {
  if (!m_expanded) {
    unsetCursor();
    return;
  }

  if (mouseIsOverResizeHandle(pos)) {
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

  if (watched == m_content) {
    if (event->type() == QEvent::LayoutRequest) {
      const int hint = m_content->sizeHint().width();
      if (hint > 0) {
        setPreferredContentWidth(hint);
        fitToContentWidth();
      }
    }
  }

  return QWidget::eventFilter(watched, event);
}

void AutoHideDock::updateStripGeometry() {
  if (!m_strip)
    return;

  const int w = StripWidth;
  const int h = height();

  const int x = (m_edge == Edge::Left) ? 0 : std::max(0, width() - w);

  m_strip->setGeometry(x, 0, w, h);
}