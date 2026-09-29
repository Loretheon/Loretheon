#include "AutoHideDock.h"

#include <QCursor>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {
bool isHorizontal(AutoHideDock::Edge edge) {
  return edge == AutoHideDock::Edge::Left ||
         edge == AutoHideDock::Edge::Right;
}

bool isLeading(AutoHideDock::Edge edge) {
  return edge == AutoHideDock::Edge::Left ||
         edge == AutoHideDock::Edge::Top;
}

const char *edgeName(AutoHideDock::Edge edge) {
  switch (edge) {
  case AutoHideDock::Edge::Left:
    return "Left";
  case AutoHideDock::Edge::Right:
    return "Right";
  case AutoHideDock::Edge::Top:
    return "Top";
  case AutoHideDock::Edge::Bottom:
    return "Bottom";
  }
  return "Left";
}

// The small widget that lives inside the strip. A short bar whose
// colour reflects the pin state. Unpinned is the theme's muted text,
// pinned is the theme's error colour. Purely decorative; the context
// menu is what actually toggles pin state.
class ChevronGrip : public QWidget {
public:
  explicit ChevronGrip(Qt::Orientation orientation, QWidget *parent = nullptr)
      : QWidget(parent), m_orientation(orientation) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setFixedSize(m_orientation == Qt::Horizontal ? QSize(4, 28)
                                                 : QSize(28, 4));
  }

  void setLocked(bool locked) {
    if (m_locked == locked)
      return;
    m_locked = locked;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor c = m_locked ? QColor("#CC79A7") : QColor("#7A96B4");

    p.setPen(Qt::NoPen);
    p.setBrush(c);

    if (m_orientation == Qt::Horizontal)
      p.drawRoundedRect(rect(), 1, 1);
    else
      p.drawRoundedRect(rect(), 1, 1);
  }

private:
  Qt::Orientation m_orientation;
  bool m_locked = false;
};
}
AutoHideDock::AutoHideDock(Edge edge, const QString &settingsPrefix,
                           QWidget *parent)
    : QWidget(parent), m_edge(edge), m_settingsPrefix(settingsPrefix) {
  setObjectName(QStringLiteral("autoHideDock%1").arg(QLatin1String(edgeName(edge))));

  setAttribute(Qt::WA_StyledBackground, true);
  setMouseTracking(true);

  loadPersistedState();

  m_hideTimer = new QTimer(this);
  m_hideTimer->setSingleShot(true);
  m_hideTimer->setInterval(kDefaultHideDelayMs);

  connect(m_hideTimer, &QTimer::timeout, this, [this]() {
    if (m_pin != Pin::None)
      return;

    if (mouseIsOverDock() || mouseIsOverStrip())
      return;

    if (m_resizing)
      return;

    hideDock();
  });

  m_strip = new QWidget(this);
  m_strip->setObjectName(
      QStringLiteral("autoHideStrip%1").arg(QLatin1String(edgeName(edge))));
  m_strip->setAttribute(Qt::WA_StyledBackground, true);
  m_strip->setMouseTracking(true);
  m_strip->setCursor(Qt::PointingHandCursor);
  m_strip->setContextMenuPolicy(Qt::CustomContextMenu);
  m_strip->installEventFilter(this);

  m_stripChevron = new ChevronGrip(orientation(), m_strip);
  m_stripChevron->setObjectName(QStringLiteral("autoHideStripGrip"));

  auto *stripLayout = new QVBoxLayout(m_strip);
  stripLayout->setContentsMargins(0, 0, 0, 0);
  stripLayout->setSpacing(0);
  stripLayout->addStretch(1);
  stripLayout->addWidget(m_stripChevron, 0, Qt::AlignCenter);
  stripLayout->addStretch(1);

  connect(m_strip, &QWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            showStripContextMenu(m_strip->mapToGlobal(pos));
          });

  connect(this, &AutoHideDock::pinChanged, this,
          [this](Pin) { refreshStripVisuals(); });

  updateStripGeometry();
  refreshStripVisuals();
  m_strip->setVisible(!m_expanded);
}

Qt::Orientation AutoHideDock::orientation() const {
  return isHorizontal(m_edge) ? Qt::Horizontal : Qt::Vertical;
}

QString AutoHideDock::lengthSettingsKey() const {
  return m_settingsPrefix +
         QStringLiteral("/length%1").arg(QLatin1String(edgeName(m_edge)));
}

QString AutoHideDock::overrideSettingsKey() const {
  return m_settingsPrefix +
         QStringLiteral("/lengthOverride%1").arg(QLatin1String(edgeName(m_edge)));
}

QString AutoHideDock::pinSettingsKey() const {
  return m_settingsPrefix +
         QStringLiteral("/pin%1").arg(QLatin1String(edgeName(m_edge)));
}

QString AutoHideDock::expandedSettingsKey() const {
  return m_settingsPrefix +
         QStringLiteral("/expanded%1").arg(QLatin1String(edgeName(m_edge)));
}

void AutoHideDock::loadPersistedState() {
  QSettings settings;

  int stored = settings.value(lengthSettingsKey(), 0).toInt();

  if (stored <= 0)
    stored = 280;

  m_dockLength = std::clamp(stored, MinDockLength, m_maxDockLength);

  m_userOverrideLength =
      settings.value(overrideSettingsKey(), false).toBool();

  const int pinInt = settings.value(pinSettingsKey(), 0).toInt();

  switch (pinInt) {
  case 1:
    m_pin = Pin::Open;
    break;
  case 2:
    m_pin = Pin::Closed;
    break;
  default:
    m_pin = Pin::None;
    break;
  }

  m_expanded = settings.value(expandedSettingsKey(), true).toBool();
}

void AutoHideDock::persistLength() {
  QSettings settings;
  settings.setValue(lengthSettingsKey(), m_dockLength);
}

void AutoHideDock::persistOverride() {
  QSettings settings;
  settings.setValue(overrideSettingsKey(), m_userOverrideLength);
}

void AutoHideDock::persistPin() {
  QSettings settings;

  int value = 0;

  if (m_pin == Pin::Open)
    value = 1;
  else if (m_pin == Pin::Closed)
    value = 2;

  settings.setValue(pinSettingsKey(), value);
}

void AutoHideDock::persistExpanded() {
  QSettings settings;
  settings.setValue(expandedSettingsKey(), m_expanded);
}

void AutoHideDock::setMaxDockLength(int length) {
  int clamped = std::max(MinDockLength, length);

  if (m_naturalLength > 0)
    clamped = std::min(clamped, m_naturalLength);

  if (m_maxDockLength == clamped)
    return;

  m_maxDockLength = clamped;

  if (m_dockLength > m_maxDockLength)
    setDockLength(m_maxDockLength);
}

void AutoHideDock::setDockLength(int length) {
  const int clamped = std::clamp(length, MinDockLength, m_maxDockLength);

  if (m_dockLength == clamped)
    return;

  m_dockLength = clamped;

  persistLength();

  emit dockLengthChanged(m_dockLength);

  if (m_expanded)
    updateGeometry();
}

void AutoHideDock::setPreferredContentLength(int length) {
  if (length <= 0) {
    m_preferredContentLength = 0;
    return;
  }

  m_preferredContentLength = length;
}

void AutoHideDock::fitToContentLength() {
  if (m_userOverrideLength)
    return;

  if (m_preferredContentLength <= 0)
    return;

  m_naturalLength = m_preferredContentLength;

  setMaxDockLength(m_naturalLength);
  setDockLength(m_naturalLength);
}

void AutoHideDock::clearUserOverrideLength() {
  if (!m_userOverrideLength)
    return;

  m_userOverrideLength = false;
  m_naturalLength = 0;

  persistOverride();

  fitToContentLength();
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

  if (m_strip)
    m_strip->raise();
}

void AutoHideDock::setPin(Pin pin) {
  if (m_pin == pin)
    return;

  m_pin = pin;

  persistPin();

  emit pinChanged(m_pin);

  if (m_pin == Pin::Open) {
    cancelHide();
    showDockImmediate();
  } else {
    hideDockImmediate();
  }

  refreshStripVisuals();
}

void AutoHideDock::showDock() {
  if (m_expanded)
    return;

  if (m_pin == Pin::Closed)
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

  if (m_pin == Pin::Open)
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

  if (m_pin == Pin::Closed)
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
  if (m_pin != Pin::None)
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

  const bool leading = isLeading(m_edge);

  if (orientation() == Qt::Horizontal) {
    return leading ? localPos.x() >= width() - ResizeHandleWidth
                   : localPos.x() <= ResizeHandleWidth;
  }

  return leading ? localPos.y() >= height() - ResizeHandleWidth
                 : localPos.y() <= ResizeHandleWidth;
}

void AutoHideDock::enterEvent(QEnterEvent *event) {
  Q_UNUSED(event);
  cancelHide();
}

void AutoHideDock::leaveEvent(QEvent *event) {
  Q_UNUSED(event);

  if (m_pin == Pin::Open)
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

  if (orientation() == Qt::Horizontal) {
    if (m_edge == Edge::Left) {
      m_content->setGeometry(r.left(), r.top(),
                             std::max(0, r.width() - ResizeHandleWidth),
                             r.height());
    } else {
      m_content->setGeometry(r.left() + ResizeHandleWidth, r.top(),
                             std::max(0, r.width() - ResizeHandleWidth),
                             r.height());
    }
    return;
  }

  if (m_edge == Edge::Top) {
    m_content->setGeometry(r.left(), r.top(), r.width(),
                           std::max(0, r.height() - ResizeHandleWidth));
  } else {
    m_content->setGeometry(r.left(), r.top() + ResizeHandleWidth, r.width(),
                           std::max(0, r.height() - ResizeHandleWidth));
  }
}

void AutoHideDock::mouseMoveEvent(QMouseEvent *event) {
  if (!event)
    return;

  if (m_resizing) {
    const int globalPos = (orientation() == Qt::Horizontal)
                              ? event->globalPosition().toPoint().x()
                              : event->globalPosition().toPoint().y();

    const int delta = globalPos - m_resizeStartGlobalPos;

    const int newLength = isLeading(m_edge)
                              ? m_resizeStartLength + delta
                              : m_resizeStartLength - delta;

    if (!m_userOverrideLength) {
      m_userOverrideLength = true;
      persistOverride();
    }

    setDockLength(newLength);
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
  m_resizeStartLength = m_dockLength;
  m_resizeStartGlobalPos = (orientation() == Qt::Horizontal)
                               ? event->globalPosition().toPoint().x()
                               : event->globalPosition().toPoint().y();
}

void AutoHideDock::mouseReleaseEvent(QMouseEvent *event) {
  Q_UNUSED(event);

  if (m_resizing) {
    m_resizing = false;
    persistLength();
    updateCursor(mapFromGlobal(QCursor::pos()));
  }
}

void AutoHideDock::contextMenuEvent(QContextMenuEvent *event) {
  if (!event)
    return;

  showStripContextMenu(event->globalPos());
  event->accept();
}

void AutoHideDock::paintEvent(QPaintEvent *event) {
  QWidget::paintEvent(event);

  if (!m_expanded)
    return;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const int barThickness = 2;
  const int barRun = 28;

  QRect grip;

  if (orientation() == Qt::Horizontal) {
    const int x = (m_edge == Edge::Left)
                      ? width() - (ResizeHandleWidth / 2) - (barThickness / 2)
                      : (ResizeHandleWidth / 2) - (barThickness / 2);

    const int y = (height() - barRun) / 2;

    grip = QRect(x, y, barThickness, barRun);
  } else {
    const int y = (m_edge == Edge::Top)
                      ? height() - (ResizeHandleWidth / 2) - (barThickness / 2)
                      : (ResizeHandleWidth / 2) - (barThickness / 2);

    const int x = (width() - barRun) / 2;

    grip = QRect(x, y, barRun, barThickness);
  }

  QColor colour = palette().color(QPalette::Mid);
  colour.setAlpha(m_hoveringHandle ? 200 : 90);

  painter.setPen(Qt::NoPen);
  painter.setBrush(colour);
  painter.drawRoundedRect(grip, 1, 1);
}

void AutoHideDock::updateCursor(const QPoint &pos) {
  if (!m_expanded) {
    unsetCursor();
    return;
  }

  if (mouseIsOverResizeHandle(pos)) {
    setCursor(orientation() == Qt::Horizontal ? Qt::SizeHorCursor
                                              : Qt::SizeVerCursor);
  } else {
    unsetCursor();
  }
}

void AutoHideDock::refreshStripVisuals() {
  if (!m_strip)
    return;

  switch (m_pin) {
  case Pin::Open:
    m_strip->setToolTip(tr("Pinned open. Right-click to change."));
    break;
  case Pin::Closed:
    m_strip->setToolTip(tr("Pinned closed. Right-click to change."));
    break;
  case Pin::None:
    m_strip->setToolTip(tr("Click to open, right-click to pin."));
    break;
  }

  auto *grip = static_cast<ChevronGrip *>(m_stripChevron);

  if (grip)
    grip->setLocked(m_pin != Pin::None);

  m_strip->update();
}

void AutoHideDock::showStripContextMenu(const QPoint &globalPos) {
  QMenu menu;

  QAction *pinOpen = menu.addAction(tr("Pin open"));
  pinOpen->setCheckable(true);
  pinOpen->setChecked(m_pin == Pin::Open);

  QAction *pinClosed = menu.addAction(tr("Pin closed"));
  pinClosed->setCheckable(true);
  pinClosed->setChecked(m_pin == Pin::Closed);

  QAction *unpin = menu.addAction(tr("Unpin"));
  unpin->setCheckable(true);
  unpin->setChecked(m_pin == Pin::None);

  menu.addSeparator();

  QAction *fit = menu.addAction(tr("Fit to content"));
  fit->setEnabled(!m_userOverrideLength && m_preferredContentLength > 0);

  QAction *reset = menu.addAction(tr("Reset length"));
  reset->setEnabled(m_userOverrideLength);

  QAction *chosen = menu.exec(globalPos);

  if (!chosen)
    return;

  if (chosen == pinOpen) {
    setPin(Pin::Open);
    return;
  }

  if (chosen == pinClosed) {
    setPin(Pin::Closed);
    return;
  }

  if (chosen == unpin) {
    setPin(Pin::None);
    return;
  }

  if (chosen == fit) {
    fitToContentLength();
    return;
  }

  if (chosen == reset) {
    clearUserOverrideLength();
    return;
  }
}

bool AutoHideDock::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_strip) {
    if (event->type() == QEvent::Enter) {
      m_hoveringStrip = true;
      m_strip->update();

      cancelHide();

      if (m_pin != Pin::Closed)
        showDock();

      return false;
    }

    if (event->type() == QEvent::Leave) {
      m_hoveringStrip = false;
      m_strip->update();

      if (m_pin == Pin::None && !mouseIsOverDock())
        scheduleHide();

      return false;
    }
  }

  if (watched == m_content) {
    if (event->type() == QEvent::LayoutRequest) {
      const int hint = (orientation() == Qt::Horizontal)
                           ? m_content->sizeHint().width()
                           : m_content->sizeHint().height();

      if (hint > 0) {
        setPreferredContentLength(hint);
        fitToContentLength();
      }
    }
  }

  return QWidget::eventFilter(watched, event);
}

void AutoHideDock::updateStripGeometry() {
  if (!m_strip)
    return;

  if (orientation() == Qt::Horizontal) {
    const int w = StripWidth;
    const int h = height();
    const int x = (m_edge == Edge::Left) ? 0 : std::max(0, width() - w);
    m_strip->setGeometry(x, 0, w, h);
  } else {
    const int w = width();
    const int h = StripWidth;
    const int y = (m_edge == Edge::Top) ? 0 : std::max(0, height() - h);
    m_strip->setGeometry(0, y, w, h);
  }
}