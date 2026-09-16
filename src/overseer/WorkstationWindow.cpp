#include "../../include/overseer/WorkstationWindow.h"

#include "../../include/app/theme/ThemeTokens.h"
#include "TextDocument.h"
#include "TextWidget.h"

#include "../../include/ai/edit/EditSession.h"

#include <QActionGroup>
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

void WorkstationWindow::setEditSession(EditSession *session) {
  m_editSession = session;
}

void WorkstationWindow::setMode(Mode mode) {
  if (m_mode == mode)
    return;

  m_mode = mode;

  if (m_mode == Mode::Tiled && m_maximized)
    toggleMaximize();

  setProperty("mode", m_mode == Mode::Tiled ? QStringLiteral("tiled")
                                            : QStringLiteral("floating"));
  style()->unpolish(this);
  style()->polish(this);
  update();

  emit modeChangeRequested(this, m_mode);
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

  m_closeButton = new QPushButton(QStringLiteral("\u2715"), header);
  m_closeButton->setObjectName(QStringLiteral("workstationWindowBtn"));
  m_closeButton->setFixedSize(24, 20);
  m_closeButton->setToolTip(tr("Close (Ctrl+W)"));

  auto *layout = new QHBoxLayout(header);
  layout->setContentsMargins(10, 2, 4, 2);
  layout->setSpacing(6);
  layout->addWidget(m_titleLabel);
  layout->addStretch(1);
  layout->addWidget(m_closeButton);

  connect(m_closeButton, &QPushButton::clicked, this,
          [this]() { emit closeRequested(this); });

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

  if (m_document->isModified())
    setStatusText(tr("Modified"));
  else
    setStatusText(tr("Editor"));
}

void WorkstationWindow::setStatusText(const QString &status) {
  m_statusText = status;

  setProperty("status", status.toLower());
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void WorkstationWindow::setAlsoOpenElsewhere(bool alsoOpen) {
  if (m_alsoOpenElsewhere == alsoOpen)
    return;

  m_alsoOpenElsewhere = alsoOpen;

  QString title = shortFileName(m_filePath);

  if (alsoOpen)
    title += QStringLiteral(" · also open");

  if (m_titleLabel)
    m_titleLabel->setText(title);
}

void WorkstationWindow::setFocused(bool focused) {
  setProperty("focused", focused);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void WorkstationWindow::setDragOverHighlight(bool highlighted) {
  if (m_dragOverHighlight == highlighted)
    return;

  m_dragOverHighlight = highlighted;

  setProperty("dragOver", highlighted);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void WorkstationWindow::setDropTargetHighlight(bool highlighted) {
  if (m_dropTargetHighlight == highlighted)
    return;

  m_dropTargetHighlight = highlighted;

  setProperty("dropTarget", highlighted);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void WorkstationWindow::toggleMaximize() {
  if (!parentWidget())
    return;

  if (m_mode == Mode::Tiled)
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

  if (!m_maximized && m_mode == Mode::Floating)
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

  const QPoint headerLocal = m_header->mapFrom(this, pos);

  if (m_closeButton && m_closeButton->geometry().contains(headerLocal))
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

    if (m_mode == Mode::Tiled)
      return;

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

  if (m_mode == Mode::Tiled) {
    if (isDragHandlePoint(event->position().toPoint()))
      setCursor(Qt::OpenHandCursor);
    else
      unsetCursor();
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

  if (m_dragging) {
    m_dragging = false;
    unsetCursor();
    emit dragFinished(this, QCursor::pos());
    return;
  }

  if (m_resizing) {
    m_resizing = false;
    m_resizeEdge = ResizeEdge::None;
    unsetCursor();
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
  qreal borderWidth = focused ? 1.5 : 1.0;

  if (m_mode == Mode::Tiled) {
    border = focused ? tokens.blue : tokens.overlay1;
    borderWidth = focused ? 1.5 : 1.0;
  } else {
    border = focused ? tokens.mauve : tokens.overlay0;
    borderWidth = focused ? 1.5 : 1.0;
  }

  if (m_dropTargetHighlight) {
    border = tokens.green;
    borderWidth = 2.5;
  } else if (m_dragOverHighlight) {
    border = tokens.peach;
    borderWidth = 2.0;
  }

  painter.setBrush(bg);
  painter.setPen(QPen(border, borderWidth));

  painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);
}

void WorkstationWindow::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);

  auto *textWidget = qobject_cast<TextWidget *>(m_body);

  if (textWidget) {
    QAction *editMode = menu.addAction(tr("Edit"));
    QAction *viewMode = menu.addAction(tr("View"));

    editMode->setCheckable(true);
    viewMode->setCheckable(true);
    editMode->setChecked(!textWidget->isViewMode());
    viewMode->setChecked(textWidget->isViewMode());

    auto *modeGroup = new QActionGroup(&menu);
    modeGroup->addAction(editMode);
    modeGroup->addAction(viewMode);
    modeGroup->setExclusive(true);

    connect(editMode, &QAction::triggered, this, [textWidget]() {
      textWidget->setViewMode(false);
    });

    connect(viewMode, &QAction::triggered, this, [textWidget]() {
      textWidget->setViewMode(true);
    });

    menu.addSeparator();
  }

  QAction *rewrite = menu.addAction(tr("Rewrite with Overseer…"));
  rewrite->setEnabled(m_editSession != nullptr);

  menu.addSeparator();

  QAction *tile = menu.addAction(tr("Tile"));
  QAction *floating = menu.addAction(tr("Float"));

  tile->setCheckable(true);
  floating->setCheckable(true);
  tile->setChecked(m_mode == Mode::Tiled);
  floating->setChecked(m_mode == Mode::Floating);

  auto *windowModeGroup = new QActionGroup(&menu);
  windowModeGroup->addAction(tile);
  windowModeGroup->addAction(floating);
  windowModeGroup->setExclusive(true);

  connect(tile, &QAction::triggered, this, [this]() {
    emit modeChangeRequested(this, Mode::Tiled);
  });

  connect(floating, &QAction::triggered, this, [this]() {
    emit modeChangeRequested(this, Mode::Floating);
  });

  menu.addSeparator();

  QAction *reload = menu.addAction(tr("Reload from disk"));
  QAction *save = menu.addAction(tr("Save"));

  menu.addSeparator();

  QAction *maximize = menu.addAction(m_maximized ? tr("Restore")
                                                 : tr("Maximize"));
  maximize->setEnabled(m_mode == Mode::Floating);

  QAction *close = menu.addAction(tr("Close"));
  QAction *closeAll = menu.addAction(tr("Close all"));

  menu.addSeparator();

  QAction *tileAll = menu.addAction(tr("Tile all"));
  QAction *floatAll = menu.addAction(tr("Float all"));
  QAction *arrangeAll = menu.addAction(tr("Arrange all"));

  QAction *chosen = menu.exec(event->globalPos());

  if (!chosen)
    return;

  if (chosen == rewrite) {
    emit rewriteRequested(this);
  } else if (chosen == reload) {
    emit diskConflictReloadRequested(this);
  } else if (chosen == save) {
    emit diskConflictOverwriteRequested(this);
  } else if (chosen == close) {
    emit closeRequested(this);
  } else if (chosen == closeAll) {
    emit closeAllRequested();
  } else if (chosen == maximize) {
    toggleMaximize();
  } else if (chosen == tileAll) {
    emit tileAllRequested();
  } else if (chosen == floatAll) {
    emit floatAllRequested();
  } else if (chosen == arrangeAll) {
    emit autoArrangeRequested();
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
  m_dragOffset = globalPos - mapToGlobal(QPoint(0, 0));
  m_dragGlobalStart = globalPos;

  setCursor(Qt::ClosedHandCursor);

  emit dragStarted(this);
}

void WorkstationWindow::beginResize(const QPoint &globalPos, ResizeEdge edge) {
  if (m_maximized)
    return;

  if (m_mode == Mode::Tiled)
    return;

  m_resizing = true;
  m_resizeEdge = edge;
  m_resizeStartGeometry = geometry();
  m_resizeStartGlobal = globalPos;
}

void WorkstationWindow::applyDrag(const QPoint &globalPos) {
  if (!parentWidget())
    return;

  const QPoint newGlobalTopLeft = globalPos - m_dragOffset;

  const QPoint newParentLocal =
      parentWidget()->mapFromGlobal(newGlobalTopLeft);

  if (m_mode == Mode::Tiled) {
    move(newParentLocal);
    emit dragMoved(this, globalPos);
    return;
  }

  constexpr int minVisible = 60;

  const int minX = -(width() - minVisible);
  const int minY = 0;

  const int maxX = parentWidget()->width() - minVisible;
  const int maxY = parentWidget()->height() - minVisible;

  QPoint pos = newParentLocal;

  pos.setX(qBound(minX, pos.x(), qMax(minX, maxX)));
  pos.setY(qBound(minY, pos.y(), qMax(minY, maxY)));

  move(pos);

  emit dragMoved(this, globalPos);
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