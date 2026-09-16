#include "../../include/overseer/TranscriptRibbon.h"

#include "../../include/app/theme/ThemeTokens.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

namespace {

QString roleForEvent(TranscriptEvent::Type type, bool toolOk) {
  switch (type) {
  case TranscriptEvent::Type::UserMessage:
    return QStringLiteral("event.user");
  case TranscriptEvent::Type::AssistantMessage:
    return QStringLiteral("event.assistant");
  case TranscriptEvent::Type::ToolCall:
  case TranscriptEvent::Type::ToolResult:
    return toolOk ? QStringLiteral("event.tool.ok")
                  : QStringLiteral("event.tool.error");
  case TranscriptEvent::Type::MemoryProposal:
    return QStringLiteral("event.proposal");
  case TranscriptEvent::Type::Stage:
    return QStringLiteral("event.stage");
  case TranscriptEvent::Type::Promotion:
    return QStringLiteral("event.promotion");
  case TranscriptEvent::Type::Error:
    return QStringLiteral("event.error");
  case TranscriptEvent::Type::Notice:
    return QStringLiteral("event.notice");
  }
  return QStringLiteral("event.notice");
}

} // namespace

TranscriptRibbon::TranscriptRibbon(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("transcriptRibbon"));
  setFixedHeight(kRibbonHeight);
  setMinimumWidth(200);
  setCursor(Qt::PointingHandCursor);
  setMouseTracking(true);
}

void TranscriptRibbon::setEvents(const QList<TranscriptEvent> &events) {
  m_events = events;
  m_total = events.size();

  if (m_total <= 0) {
    m_visibleStart = 0;
    m_visibleEnd = 0;
  } else {
    m_visibleStart = qBound(0, m_visibleStart, m_total - 1);
    m_visibleEnd = qBound(m_visibleStart, m_visibleEnd, m_total - 1);
  }

  update();
}

void TranscriptRibbon::setVisibleRange(int startIndex, int endIndex) {
  if (m_total <= 0)
    return;

  const int s = qBound(0, startIndex, m_total - 1);
  const int e = qBound(s, endIndex, m_total - 1);

  if (s == m_visibleStart && e == m_visibleEnd)
    return;

  m_visibleStart = s;
  m_visibleEnd = e;

  update();
}

int TranscriptRibbon::xForIndex(int index) const {
  if (m_total <= 0)
    return 0;

  const int usable = qMax(1, width());
  const double tickWidth = static_cast<double>(usable) / m_total;

  return static_cast<int>(index * tickWidth);
}

int TranscriptRibbon::xForIndexEnd(int index) const {
  if (m_total <= 0)
    return 0;

  const int usable = qMax(1, width());
  const double tickWidth = static_cast<double>(usable) / m_total;

  return static_cast<int>((index + 1) * tickWidth);
}

int TranscriptRibbon::indexAtX(int x) const {
  if (m_total <= 0)
    return 0;

  const int usable = qMax(1, width());
  const double tickWidth = static_cast<double>(usable) / m_total;

  return qBound(0, static_cast<int>(x / tickWidth), m_total - 1);
}

void TranscriptRibbon::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  update();
}

void TranscriptRibbon::changeEvent(QEvent *event) {
  if (event && (event->type() == QEvent::PaletteChange ||
                event->type() == QEvent::StyleChange)) {
    update();
  }
  QWidget::changeEvent(event);
}

void TranscriptRibbon::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);

  const QColor base = palette().color(QPalette::Base);
  const QColor highlight = palette().color(QPalette::Highlight);

  p.fillRect(rect(), base.darker(108));

  if (m_total <= 0)
    return;

  for (int i = 0; i < m_total; ++i) {
    const int x0 = xForIndex(i);
    const int x1 = xForIndexEnd(i);

    QColor c = ThemeRegistry::instance().color(
        roleForEvent(m_events[i].type, m_events[i].toolOk));

    p.fillRect(QRect(x0, 4, qMax(1, x1 - x0 - kTickGap), height() - 8), c);
  }

  const int cursorLeft = xForIndex(m_visibleStart);
  const int cursorRight = xForIndexEnd(m_visibleEnd);

  const int left = qMax(0, cursorLeft);
  const int right = qMin(width(), cursorRight);

  QColor cursorFill = highlight;
  cursorFill.setAlpha(60);
  p.fillRect(QRect(left, 0, qMax(2, right - left), height()), cursorFill);

  QColor cursorEdge = highlight;
  cursorEdge.setAlpha(255);
  p.setPen(Qt::NoPen);
  p.setBrush(cursorEdge);

  p.fillRect(QRect(left, 0, kCursorWidthPx, height()), cursorEdge);
  p.fillRect(QRect(qMax(left, right - kCursorWidthPx), 0, kCursorWidthPx,
                   height()),
             cursorEdge);
}

void TranscriptRibbon::mousePressEvent(QMouseEvent *event) {
  if (!event)
    return;

  const int x = event->position().toPoint().x();

  const int cursorLeft = xForIndex(m_visibleStart);
  const int cursorRight = xForIndexEnd(m_visibleEnd);

  if (x >= cursorLeft - kCursorHitZonePx && x <= cursorRight + kCursorHitZonePx) {
    m_draggingCursor = true;
    setCursor(Qt::SizeHorCursor);
    emit jumpRequested(indexAtX(x));
    return;
  }

  emit jumpRequested(indexAtX(x));
}

void TranscriptRibbon::mouseMoveEvent(QMouseEvent *event) {
  if (!event)
    return;

  const int x = event->position().toPoint().x();

  if (!m_draggingCursor) {
    const int cursorLeft = xForIndex(m_visibleStart);
    const int cursorRight = xForIndexEnd(m_visibleEnd);

    if (x >= cursorLeft - kCursorHitZonePx &&
        x <= cursorRight + kCursorHitZonePx) {
      setCursor(Qt::SizeHorCursor);
    } else {
      setCursor(Qt::PointingHandCursor);
    }
    return;
  }

  emit jumpRequested(indexAtX(x));
}

void TranscriptRibbon::mouseReleaseEvent(QMouseEvent *event) {
  Q_UNUSED(event);

  m_draggingCursor = false;
  setCursor(Qt::PointingHandCursor);
}