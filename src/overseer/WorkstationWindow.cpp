#include "../../include/overseer/WorkstationWindow.h"

#include "../../include/app/theme/ThemeTokens.h"
#include "TextDocument.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString shortFileName(const QString &path) {
  const int slash = path.lastIndexOf(QChar('/'));

  if (slash < 0)
    return path;

  return path.mid(slash + 1);
}

} // namespace

WorkstationWindow::WorkstationWindow(TextDocument *document, QWidget *body,
                                     const QString &filePath, QWidget *parent)
    : QWidget(parent),
      m_document(document),
      m_body(body),
      m_filePath(filePath) {
  setObjectName(QStringLiteral("workstationWindow"));
  setAttribute(Qt::WA_StyledBackground, true);
  setMouseTracking(true);
  setMinimumSize(kMinimumWidth, kMinimumHeight);
  setFocusPolicy(Qt::StrongFocus);

  if (m_body)
    m_body->setParent(this);

  m_header = buildHeader();
  m_conflictBanner = buildConflictBanner();

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(1, 1, 1, 1);
  layout->setSpacing(0);
  layout->addWidget(m_header);
  layout->addWidget(m_conflictBanner);

  if (m_body)
    layout->addWidget(m_body, 1);

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(18);
  shadow->setOffset(0, 4);
  shadow->setColor(QColor(0, 0, 0, 110));

  setGraphicsEffect(shadow);

  if (m_document) {
    connect(m_document, &QTextDocument::modificationChanged, this,
            [this](bool) { refreshModifiedIndicator(); });
  }

  refreshModifiedIndicator();
}

WorkstationWindow::~WorkstationWindow() = default;

QString WorkstationWindow::displayName() const {
  return shortFileName(m_filePath);
}

QSize WorkstationWindow::preferredSize() const {
  return QSize(480, 360);
}

QWidget *WorkstationWindow::buildHeader() {
  auto *header = new QWidget(this);
  header->setObjectName(QStringLiteral("workstationWindowHeader"));
  header->setFixedHeight(kHeaderHeight);
  header->setAttribute(Qt::WA_StyledBackground, true);
  header->setCursor(Qt::OpenHandCursor);

  m_titleLabel = new QLabel(shortFileName(m_filePath), header);
  m_titleLabel->setObjectName(QStringLiteral("workstationWindowTitle"));
  m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  m_alsoOpenBadge = new QLabel(tr("Also open"), header);
  m_alsoOpenBadge->setObjectName(QStringLiteral("workstationWindowBadge"));
  m_alsoOpenBadge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_alsoOpenBadge->setVisible(false);

  m_statusPill = new QLabel(tr("Editor"), header);
  m_statusPill->setObjectName(QStringLiteral("workstationWindowPill"));
  m_statusPill->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  m_maximizeButton = new QPushButton(QStringLiteral("\u25A1"), header);
  m_maximizeButton->setObjectName(QStringLiteral("workstationWindowBtn"));
  m_maximizeButton->setFixedSize(26, 22);
  m_maximizeButton->setToolTip(tr("Maximize / restore"));

  m_closeButton = new QPushButton(QStringLiteral("\u2715"), header);
  m_closeButton->setObjectName(QStringLiteral("workstationWindowBtn"));
  m_closeButton->setFixedSize(26, 22);
  m_closeButton->setToolTip(tr("Close (Ctrl+W)"));

  auto *layout = new QHBoxLayout(header);
  layout->setContentsMargins(10, 2, 6, 2);
  layout->setSpacing(6);
  layout->addWidget(m_titleLabel);
  layout->addWidget(m_alsoOpenBadge);
  layout->addStretch(1);
  layout->addWidget(m_statusPill);
  layout->addWidget(m_maximizeButton);
  layout->addWidget(m_closeButton);

  connect(m_closeButton, &QPushButton::clicked, this,
          [this]() { emit closeRequested(this); });

  connect(m_maximizeButton, &QPushButton::clicked, this,
          [this]() { toggleMaximize(); });

  return header;
}

QWidget *WorkstationWindow::buildConflictBanner() {
  auto *banner = new QWidget(this);
  banner->setObjectName(QStringLiteral("workstationDiskConflictBanner"));
  banner->setAttribute(Qt::WA_StyledBackground, true);
  banner->setVisible(false);

  auto *label = new QLabel(
      tr("This file changed on disk. Unsaved edits present."), banner);
  label->setObjectName(QStringLiteral("workstationDiskConflictLabel"));

  auto *reloadButton = new QPushButton(tr("Reload"), banner);
  auto *keepButton = new QPushButton(tr("Keep mine"), banner);
  auto *overwriteButton = new QPushButton(tr("Overwrite"), banner);

  reloadButton->setObjectName(QStringLiteral("workstationConflictBtn"));
  keepButton->setObjectName(QStringLiteral("workstationConflictBtn"));
  overwriteButton->setObjectName(QStringLiteral("workstationConflictBtn"));

  auto *layout = new QHBoxLayout(banner);
  layout->setContentsMargins(10, 4, 6, 4);
  layout->setSpacing(6);
  layout->addWidget(label, 1);
  layout->addWidget(reloadButton);
  layout->addWidget(keepButton);
  layout->addWidget(overwriteButton);

  connect(reloadButton, &QPushButton::clicked, this,
          [this]() { emit diskConflictReloadRequested(this); });

  connect(keepButton, &QPushButton::clicked, this,
          [this]() { emit diskConflictKeepMineRequested(this); });

  connect(overwriteButton, &QPushButton::clicked, this,
          [this]() { emit diskConflictOverwriteRequested(this); });

  return banner;
}

void WorkstationWindow::showDiskConflictBanner() {
  if (m_conflictBanner)
    m_conflictBanner->setVisible(true);
}

void WorkstationWindow::hideDiskConflictBanner() {
  if (m_conflictBanner)
    m_conflictBanner->setVisible(false);
}

void WorkstationWindow::refreshModifiedIndicator() {
  if (!m_document)
    return;

  if (m_document->isModified()) {
    setStatusPill(tr("Modified"));
  } else {
    setStatusPill(tr("Editor"));
  }
}

void WorkstationWindow::setStatusPill(const QString &status) {
  if (!m_statusPill)
    return;

  m_statusPill->setText(status);
  m_statusPill->setProperty("status", status.toLower());
  m_statusPill->style()->unpolish(m_statusPill);
  m_statusPill->style()->polish(m_statusPill);
}

void WorkstationWindow::setAlsoOpenElsewhere(bool alsoOpen) {
  if (m_alsoOpenBadge)
    m_alsoOpenBadge->setVisible(alsoOpen);
}

void WorkstationWindow::setFocused(bool focused) {
  setProperty("focused", focused);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void WorkstationWindow::toggleMaximize() {
  if (!parentWidget())
    return;

  if (!m_maximized) {
    m_restoreGeometry = geometry();
    m_maximized = true;
    setGeometry(parentWidget()->rect().adjusted(2, 2, -2, -2));
  } else {
    m_maximized = false;
    setGeometry(m_restoreGeometry);
  }

  emit geometryChanged(this);
}

void WorkstationWindow::setRestoreGeometry(const QRect &rect) {
  m_restoreGeometry = rect;

  if (!m_maximized)
    setGeometry(rect);
}

bool WorkstationWindow::isDragHandlePoint(const QPoint &pos) const {
  if (!m_header)
    return false;

  const QRect headerRect = m_header->geometry();

  const QRect dragZone(headerRect.left(), qMax(0, headerRect.top()),
                       headerRect.width(),
                       headerRect.height() + kDragPaddingTop);

  if (!dragZone.contains(pos))
    return false;

  if (m_maximizeButton &&
      m_maximizeButton->geometry().contains(m_header->mapFrom(this, pos)))
    return false;

  if (m_closeButton &&
      m_closeButton->geometry().contains(m_header->mapFrom(this, pos)))
    return false;

  return true;
}

void WorkstationWindow::mousePressEvent(QMouseEvent *event) {
  if (!event)
    return;

  emit focusRequested(this);

  const QPoint pos = event->position().toPoint();

  if (event->button() == Qt::LeftButton) {
    if (isDragHandlePoint(pos)) {
      beginDrag(event->globalPosition().toPoint());
      return;
    }

    const ResizeEdge edge = edgeAt(pos);

    if (edge != ResizeEdge::None) {
      beginResize(event->globalPosition().toPoint(), edge);
      return;
    }
  }
}

void WorkstationWindow::mouseMoveEvent(QMouseEvent *event) {
  if (!event)
    return;

  if (m_dragging) {
    applyDrag(event->globalPosition().toPoint());
    return;
  }

  if (m_resizing) {
    applyResize(event->globalPosition().toPoint());
    return;
  }

  const QPoint pos = event->position().toPoint();

  if (isDragHandlePoint(pos)) {
    setCursor(Qt::OpenHandCursor);
    return;
  }

  updateCursorForEdge(edgeAt(pos));
}

void WorkstationWindow::mouseReleaseEvent(QMouseEvent *event) {
  Q_UNUSED(event);

  if (m_dragging || m_resizing) {
    const bool wasDragging = m_dragging;
    const bool wasResizing = m_resizing;

    m_dragging = false;
    m_resizing = false;
    m_resizeEdge = ResizeEdge::None;
    unsetCursor();

    if (wasDragging)
      emit dragFinished(this);

    if (wasResizing)
      emit resizeFinished(this);

    emit geometryChanged(this);
  }
}

void WorkstationWindow::mouseDoubleClickEvent(QMouseEvent *event) {
  if (!event)
    return;

  if (isDragHandlePoint(event->position().toPoint()))
    toggleMaximize();
}

void WorkstationWindow::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
}

void WorkstationWindow::changeEvent(QEvent *event) {
  if (event && (event->type() == QEvent::PaletteChange ||
                event->type() == QEvent::StyleChange)) {
    update();
  }
  QWidget::changeEvent(event);
}

void WorkstationWindow::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const ThemeTokens tokens =
      ThemeRegistry::instance().tokens(ThemeRegistry::instance().activeTheme());

  const bool focused = property("focused").toBool();

  QColor bg = tokens.base;
  QColor border = focused ? tokens.blue : tokens.overlay0;

  painter.setBrush(bg);
  painter.setPen(QPen(border, focused ? 1.5 : 1.0));

  painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);
}

void WorkstationWindow::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);

  QAction *reload = menu.addAction(tr("Reload from disk"));
  QAction *save = menu.addAction(tr("Save"));

  menu.addSeparator();

  QAction *close = menu.addAction(tr("Close"));
  QAction *closeAll = menu.addAction(tr("Close all"));

  menu.addSeparator();

  QAction *maximize = menu.addAction(m_maximized ? tr("Restore")
                                                 : tr("Maximize"));
  QAction *autoArrange = menu.addAction(tr("Auto-arrange"));
  QAction *tile = menu.addAction(tr("Tile"));
  QAction *cascade = menu.addAction(tr("Cascade"));

  QAction *chosen = menu.exec(event->globalPos());

  if (chosen == reload) {
    emit diskConflictReloadRequested(this);
  } else if (chosen == save) {
    emit diskConflictOverwriteRequested(this);
  } else if (chosen == close) {
    emit closeRequested(this);
  } else if (chosen == closeAll) {
    emit closeAllRequested();
  } else if (chosen == maximize) {
    toggleMaximize();
  } else if (chosen == autoArrange) {
    emit autoArrangeRequested();
  } else if (chosen == tile) {
    emit tileRequested();
  } else if (chosen == cascade) {
    emit cascadeRequested();
  }
}

void WorkstationWindow::keyPressEvent(QKeyEvent *event) {
  if (!event) {
    QWidget::keyPressEvent(event);
    return;
  }

  if (event->modifiers() == Qt::ControlModifier &&
      event->key() == Qt::Key_W) {
    emit closeRequested(this);
    return;
  }

  QWidget::keyPressEvent(event);
}

WorkstationWindow::ResizeEdge WorkstationWindow::edgeAt(
    const QPoint &pos) const {
  const int w = width();
  const int h = height();

  const bool onLeft = pos.x() <= kResizeBorder;
  const bool onRight = pos.x() >= w - kResizeBorder;
  const bool onTop = pos.y() <= kResizeBorder;
  const bool onBottom = pos.y() >= h - kResizeBorder;

  if (onTop && onLeft) return ResizeEdge::TopLeft;
  if (onTop && onRight) return ResizeEdge::TopRight;
  if (onBottom && onLeft) return ResizeEdge::BottomLeft;
  if (onBottom && onRight) return ResizeEdge::BottomRight;
  if (onTop) return ResizeEdge::Top;
  if (onBottom) return ResizeEdge::Bottom;
  if (onLeft) return ResizeEdge::Left;
  if (onRight) return ResizeEdge::Right;

  return ResizeEdge::None;
}

void WorkstationWindow::updateCursorForEdge(ResizeEdge edge) {
  switch (edge) {
  case ResizeEdge::Top:
  case ResizeEdge::Bottom:
    setCursor(Qt::SizeVerCursor);
    break;
  case ResizeEdge::Left:
  case ResizeEdge::Right:
    setCursor(Qt::SizeHorCursor);
    break;
  case ResizeEdge::TopLeft:
  case ResizeEdge::BottomRight:
    setCursor(Qt::SizeFDiagCursor);
    break;
  case ResizeEdge::TopRight:
  case ResizeEdge::BottomLeft:
    setCursor(Qt::SizeBDiagCursor);
    break;
  case ResizeEdge::None:
    unsetCursor();
    break;
  }
}

void WorkstationWindow::beginDrag(const QPoint &globalPos) {
  if (m_maximized)
    return;

  m_dragging = true;
  m_dragOffset = globalPos - frameGeometry().topLeft();

  setCursor(Qt::ClosedHandCursor);
}

void WorkstationWindow::beginResize(const QPoint &globalPos, ResizeEdge edge) {
  if (m_maximized)
    return;

  m_resizing = true;
  m_resizeEdge = edge;
  m_resizeStartGeometry = geometry();
  m_resizeStartGlobal = globalPos;
}

void WorkstationWindow::applyDrag(const QPoint &globalPos) {
  if (!parentWidget())
    return;

  const QPoint target = globalPos - m_dragOffset;
  const QPoint localTarget = parentWidget()->mapFromGlobal(target);

  QPoint pos = localTarget;

  constexpr int minVisible = 60;

  const int minX = -(width() - minVisible);
  const int minY = 0;

  const int maxX = parentWidget()->width() - minVisible;
  const int maxY = parentWidget()->height() - minVisible;

  pos.setX(qBound(minX, pos.x(), qMax(minX, maxX)));
  pos.setY(qBound(minY, pos.y(), qMax(minY, maxY)));

  move(pos);
}

void WorkstationWindow::applyResize(const QPoint &globalPos) {
  const QPoint delta = globalPos - m_resizeStartGlobal;

  QRect g = m_resizeStartGeometry;

  switch (m_resizeEdge) {
  case ResizeEdge::Top:
    g.setTop(g.top() + delta.y());
    break;
  case ResizeEdge::Bottom:
    g.setBottom(g.bottom() + delta.y());
    break;
  case ResizeEdge::Left:
    g.setLeft(g.left() + delta.x());
    break;
  case ResizeEdge::Right:
    g.setRight(g.right() + delta.x());
    break;
  case ResizeEdge::TopLeft:
    g.setTop(g.top() + delta.y());
    g.setLeft(g.left() + delta.x());
    break;
  case ResizeEdge::TopRight:
    g.setTop(g.top() + delta.y());
    g.setRight(g.right() + delta.x());
    break;
  case ResizeEdge::BottomLeft:
    g.setBottom(g.bottom() + delta.y());
    g.setLeft(g.left() + delta.x());
    break;
  case ResizeEdge::BottomRight:
    g.setBottom(g.bottom() + delta.y());
    g.setRight(g.right() + delta.x());
    break;
  case ResizeEdge::None:
    return;
  }

  if (g.width() < kMinimumWidth) {
    if (m_resizeEdge == ResizeEdge::Left ||
        m_resizeEdge == ResizeEdge::TopLeft ||
        m_resizeEdge == ResizeEdge::BottomLeft) {
      g.setLeft(g.right() - kMinimumWidth);
    } else {
      g.setRight(g.left() + kMinimumWidth);
    }
  }

  if (g.height() < kMinimumHeight) {
    if (m_resizeEdge == ResizeEdge::Top ||
        m_resizeEdge == ResizeEdge::TopLeft ||
        m_resizeEdge == ResizeEdge::TopRight) {
      g.setTop(g.bottom() - kMinimumHeight);
    } else {
      g.setBottom(g.top() + kMinimumHeight);
    }
  }

  setGeometry(g);
}

void WorkstationWindow::clampToParent() {
  if (!parentWidget())
    return;

  QRect g = geometry();
  const QRect bounds = parentWidget()->rect();

  if (g.right() > bounds.right())
    g.moveRight(bounds.right());
  if (g.bottom() > bounds.bottom())
    g.moveBottom(bounds.bottom());
  if (g.left() < bounds.left())
    g.moveLeft(bounds.left());
  if (g.top() < bounds.top())
    g.moveTop(bounds.top());

  setGeometry(g);
}