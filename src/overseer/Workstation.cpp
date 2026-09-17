#include "../../include/overseer/Workstation.h"

#include "../../include/app/theme/ThemeTokens.h"
#include "WorkstationWindow.h"

#include "../../include/app/DocumentManager.h"

#include "../../include/ai/edit/EditSession.h"

#include "TextDocument.h"

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <limits>

namespace {

constexpr auto LayoutFilename = "workstation.json";
constexpr int LayoutVersion = 2;

} // namespace

Workstation::Workstation(DocumentManager *manager, EditSession *editSession,
                         QWidget *parent)
    : QWidget(parent), m_manager(manager), m_editSession(editSession) {
  setObjectName(QStringLiteral("workstation"));
  setAttribute(Qt::WA_StyledBackground, true);
  setAcceptDrops(false);
  setFocusPolicy(Qt::StrongFocus);

  connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this,
          &Workstation::onDiskFileChanged);
}

Workstation::~Workstation() = default;

void Workstation::setOutputFolder(const QString &folder) {
  m_outputFolder = folder;
}

QString Workstation::sessionFolder() const {
  if (m_outputFolder.isEmpty())
    return {};

  QDir dir(m_outputFolder);
  dir.cdUp();

  return dir.absolutePath();
}

QString Workstation::absolutePathFor(const QString &relativePath) const {
  if (m_outputFolder.isEmpty())
    return relativePath;

  return QDir(m_outputFolder).filePath(relativePath);
}

QString Workstation::layoutPath() const {
  const QString session = sessionFolder();

  if (session.isEmpty())
    return {};

  return QDir(session).filePath(QString::fromLatin1(LayoutFilename));
}

WorkstationWindow *Workstation::windowForPath(
    const QString &absolutePath) const {
  return m_byPath.value(absolutePath, nullptr);
}

int Workstation::nextZ() { return ++m_zCounter; }

void Workstation::watchFile(const QString &absolutePath) {
  if (absolutePath.isEmpty())
    return;

  if (!QFileInfo::exists(absolutePath))
    return;

  if (m_fileWatcher.files().contains(absolutePath))
    return;

  m_fileWatcher.addPath(absolutePath);
}

void Workstation::unwatchFile(const QString &absolutePath) {
  if (absolutePath.isEmpty())
    return;

  if (m_fileWatcher.files().contains(absolutePath))
    m_fileWatcher.removePath(absolutePath);
}

void Workstation::bringToFront(WorkstationWindow *window) {
  if (!window)
    return;

  m_focusedWindow = window;

  window->raise();
  window->setFocus(Qt::OtherFocusReason);

  const int z = nextZ();
  window->setZOrder(z);

  for (WorkstationWindow *w : std::as_const(m_windows))
    w->setFocused(w == window);

  emit currentFileChanged(window->filePath());
}

void Workstation::focusWindow(WorkstationWindow *window) {
  bringToFront(window);
}

void Workstation::onWindowFocusRequested(WorkstationWindow *window) {
  bringToFront(window);
}

void Workstation::destroyWindow(WorkstationWindow *window, bool emitSignals) {
  if (!window)
    return;

  const QString path = window->filePath();

  unwatchFile(path);

  m_windows.removeOne(window);
  m_byPath.remove(path);

  if (m_focusedWindow == window)
    m_focusedWindow = nullptr;

  if (m_dropTarget == window)
    m_dropTarget = nullptr;

  window->hide();
  window->setParent(nullptr);
  window->deleteLater();

  if (emitSignals) {
    emit windowClosed(path);
    emit windowListChanged();
  }
}

void Workstation::onWindowCloseRequested(WorkstationWindow *window) {
  if (!window)
    return;

  destroyWindow(window, true);

  updateAlsoOpenBadges();
  applyTiling();
  saveLayout();
}

void Workstation::onWindowGeometryChanged(WorkstationWindow *window) {
  if (m_loading)
    return;

  Q_UNUSED(window);
  saveLayout();
}

void Workstation::onWindowResizeFinished(WorkstationWindow *window) {
  if (m_loading)
    return;

  Q_UNUSED(window);
  saveLayout();
}

void Workstation::onWindowDragStarted(WorkstationWindow *window) {
  Q_UNUSED(window);
  clearDragHighlights();
}

void Workstation::onWindowDragMoved(WorkstationWindow *window,
                                    const QPoint &globalPos) {
  if (!window)
    return;

  if (window->mode() != WorkstationWindow::Mode::Tiled) {
    clearDragHighlights();
    return;
  }

  WorkstationWindow *nearest = nearestTiledWindow(globalPos, window);

  if (nearest != m_dropTarget) {
    if (m_dropTarget)
      m_dropTarget->setDropTargetHighlight(false);

    m_dropTarget = nearest;

    if (m_dropTarget)
      m_dropTarget->setDropTargetHighlight(true);
  }
}

void Workstation::onWindowDragFinished(WorkstationWindow *window,
                                       const QPoint &globalPos) {
  clearDragHighlights();

  if (!window)
    return;

  if (window->mode() == WorkstationWindow::Mode::Tiled) {
    WorkstationWindow *nearest = nearestTiledWindow(globalPos, window);

    if (nearest && nearest != window) {
      const int a = m_windows.indexOf(window);
      const int b = m_windows.indexOf(nearest);

      if (a >= 0 && b >= 0) {
        m_windows.swapItemsAt(a, b);
      }
    }

    applyTiling();
  }

  saveLayout();
}

void Workstation::onWindowModeChangeRequested(WorkstationWindow *window,
                                              WorkstationWindow::Mode mode) {
  if (!window)
    return;

  window->setMode(mode);

  if (mode == WorkstationWindow::Mode::Tiled) {
    m_windows.removeOne(window);
    m_windows.append(window);
  }

  applyTiling();
  saveLayout();
}

void Workstation::onDiskFileChanged(const QString &path) {
  if (m_ignoreNextChange.contains(path)) {
    m_ignoreNextChange.remove(path);

    QTimer::singleShot(0, this, [this, path]() {
      if (QFileInfo::exists(path) && !m_fileWatcher.files().contains(path))
        m_fileWatcher.addPath(path);
    });

    return;
  }

  WorkstationWindow *window = windowForPath(path);

  if (!window)
    return;

  TextDocument *doc = window->document();

  if (!doc) {
    if (QFileInfo::exists(path) && !m_fileWatcher.files().contains(path))
      m_fileWatcher.addPath(path);
    return;
  }

  if (!doc->isModified()) {
    reloadWindowFromDisk(window);
  } else {
    window->showDiskConflictBanner();
  }

  QTimer::singleShot(0, this, [this, path]() {
    if (QFileInfo::exists(path) && !m_fileWatcher.files().contains(path))
      m_fileWatcher.addPath(path);
  });
}

void Workstation::reloadWindowFromDisk(WorkstationWindow *window) {
  if (!window)
    return;

  const QString path = window->filePath();

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString content = stream.readAll();

  if (stream.status() != QTextStream::Ok)
    return;

  TextDocument *doc = window->document();

  if (!doc)
    return;

  doc->setPlainText(content);
  doc->setModified(false);

  window->hideDiskConflictBanner();
  window->setTransientStatus(WorkstationWindow::Status::ExternalChange,
                             tr("Reloaded from disk"));
}

void Workstation::saveWindowToDisk(WorkstationWindow *window) {
  if (!window || !m_manager)
    return;

  const QString path = window->filePath();

  TextDocument *doc = window->document();

  if (!doc)
    return;

  m_ignoreNextChange.insert(path);

  m_manager->saveDocument(doc);

  window->hideDiskConflictBanner();
  window->setTransientStatus(WorkstationWindow::Status::Saved, tr("Saved"));
}

void Workstation::onDiskConflictReload(WorkstationWindow *window) {
  reloadWindowFromDisk(window);
}

void Workstation::onDiskConflictKeepMine(WorkstationWindow *window) {
  if (!window)
    return;

  window->hideDiskConflictBanner();
  window->refreshNeutralStatus();
}

void Workstation::onDiskConflictOverwrite(WorkstationWindow *window) {
  saveWindowToDisk(window);
}

QList<WorkstationWindow *> Workstation::tiledWindows() const {
  QList<WorkstationWindow *> result;

  for (WorkstationWindow *w : m_windows) {
    if (w && w->mode() == WorkstationWindow::Mode::Tiled)
      result.append(w);
  }

  return result;
}

QList<WorkstationWindow *> Workstation::floatingWindows() const {
  QList<WorkstationWindow *> result;

  for (WorkstationWindow *w : m_windows) {
    if (w && w->mode() == WorkstationWindow::Mode::Floating)
      result.append(w);
  }

  return result;
}

QList<QRect> Workstation::computeTiledRects(int count) const {
  QList<QRect> result;

  if (count <= 0)
    return result;

  const int usableW = width() - kMargin * 2;
  const int usableH = height() - kMargin * 2;

  if (usableW <= 0 || usableH <= 0) {
    for (int i = 0; i < count; ++i)
      result.append(QRect(kMargin, kMargin, 200, 200));

    return result;
  }

  int cols = qCeil(qSqrt(static_cast<double>(count) *
                         (static_cast<double>(usableH) /
                          static_cast<double>(usableW))));

  cols = qBound(1, cols, count);

  const int rows = qCeil(static_cast<double>(count) / cols);

  const int cellW = (usableW - kGap * (cols - 1)) / cols;
  const int cellH = (usableH - kGap * (rows - 1)) / rows;

  for (int i = 0; i < count; ++i) {
    const int r = i / cols;
    const int c = i % cols;

    const int x = kMargin + c * (cellW + kGap);
    const int y = kMargin + r * (cellH + kGap);

    result.append(QRect(x, y, cellW, cellH));
  }

  return result;
}

void Workstation::applyTiling() {
  const QList<WorkstationWindow *> tiled = tiledWindows();

  const QList<QRect> rects = computeTiledRects(tiled.size());

  for (int i = 0; i < tiled.size() && i < rects.size(); ++i) {
    WorkstationWindow *w = tiled.at(i);

    if (!w)
      continue;

    const QRect r = rects.at(i);

    w->setGeometry(r);
    w->setRestoreGeometry(r);
  }
}

void Workstation::setAllTiled() {
  for (WorkstationWindow *w : std::as_const(m_windows)) {
    if (!w)
      continue;

    w->setMode(WorkstationWindow::Mode::Tiled);
  }

  applyTiling();
  saveLayout();
}

void Workstation::setAllFloating() {
  for (WorkstationWindow *w : std::as_const(m_windows)) {
    if (!w)
      continue;

    w->setMode(WorkstationWindow::Mode::Floating);
  }

  saveLayout();
}

void Workstation::arrangeAll() {
  applyTiling();
  saveLayout();
}

WorkstationWindow *Workstation::nearestTiledWindow(
    const QPoint &globalPos, WorkstationWindow *ignore) const {
  WorkstationWindow *best = nullptr;
  int bestDistance = std::numeric_limits<int>::max();

  for (WorkstationWindow *w : m_windows) {
    if (!w || w == ignore)
      continue;

    if (w->mode() != WorkstationWindow::Mode::Tiled)
      continue;

    const QPoint center = w->mapToGlobal(w->rect().center());
    const int dx = center.x() - globalPos.x();
    const int dy = center.y() - globalPos.y();
    const int distance = dx * dx + dy * dy;

    if (distance < bestDistance) {
      bestDistance = distance;
      best = w;
    }
  }

  return best;
}

void Workstation::clearDragHighlights() {
  for (WorkstationWindow *w : std::as_const(m_windows)) {
    if (w) {
      w->setDropTargetHighlight(false);
      w->setDragOverHighlight(false);
    }
  }

  m_dropTarget = nullptr;
}

WorkstationWindow *Workstation::openFile(const QString &absolutePath,
                                         const QString &bodyHint) {
  if (absolutePath.isEmpty())
    return nullptr;

  if (WorkstationWindow *existing = windowForPath(absolutePath)) {
    bringToFront(existing);
    return nullptr;
  }

  if (!m_manager)
    return nullptr;

  TextDocument *document = nullptr;

  for (TextDocument *candidate : m_manager->openDocuments()) {
    if (candidate->filePath() == absolutePath) {
      document = candidate;
      break;
    }
  }

  if (!document) {
    if (!m_manager->openFile(absolutePath))
      return nullptr;

    for (TextDocument *candidate : m_manager->openDocuments()) {
      if (candidate->filePath() == absolutePath) {
        document = candidate;
        break;
      }
    }
  }

  if (!document)
    return nullptr;

  const QString hint = bodyHint.isEmpty()
                           ? WorkstationBodyFactory::defaultHint()
                           : bodyHint;

  QWidget *body = m_bodyFactory.createBody(hint, document, m_editSession, this);

  auto *window = new WorkstationWindow(document, body, absolutePath, this);
  window->setEditSession(m_editSession);
  window->setMode(WorkstationWindow::Mode::Tiled);

  connect(window, &WorkstationWindow::closeRequested, this,
          &Workstation::onWindowCloseRequested);

  connect(window, &WorkstationWindow::focusRequested, this,
          &Workstation::onWindowFocusRequested);

  connect(window, &WorkstationWindow::geometryChanged, this,
          &Workstation::onWindowGeometryChanged);

  connect(window, &WorkstationWindow::dragStarted, this,
          &Workstation::onWindowDragStarted);

  connect(window, &WorkstationWindow::dragMoved, this,
          &Workstation::onWindowDragMoved);

  connect(window, &WorkstationWindow::dragFinished, this,
          &Workstation::onWindowDragFinished);

  connect(window, &WorkstationWindow::resizeFinished, this,
          &Workstation::onWindowResizeFinished);

  connect(window, &WorkstationWindow::modeChangeRequested, this,
          &Workstation::onWindowModeChangeRequested);

  connect(window, &WorkstationWindow::closeAllRequested, this,
          &Workstation::closeAll);

  connect(window, &WorkstationWindow::tileAllRequested, this,
          &Workstation::setAllTiled);

  connect(window, &WorkstationWindow::floatAllRequested, this,
          &Workstation::setAllFloating);

  connect(window, &WorkstationWindow::autoArrangeRequested, this,
          &Workstation::arrangeAll);

  connect(window, &WorkstationWindow::rewriteRequested, this,
          [this](WorkstationWindow *w) { emit rewriteRequested(w); });

  connect(window, &WorkstationWindow::diskConflictReloadRequested, this,
          &Workstation::onDiskConflictReload);

  connect(window, &WorkstationWindow::diskConflictKeepMineRequested, this,
          &Workstation::onDiskConflictKeepMine);

  connect(window, &WorkstationWindow::diskConflictOverwriteRequested, this,
          &Workstation::onDiskConflictOverwrite);

  m_windows.append(window);
  m_byPath.insert(absolutePath, window);

  watchFile(absolutePath);

  window->show();
  bringToFront(window);

  updateAlsoOpenBadges();

  applyTiling();

  if (!m_loading)
    saveLayout();

  window->setTransientStatus(WorkstationWindow::Status::Opening,
                             tr("Opened"));

  emit windowOpened(absolutePath);
  emit windowListChanged();

  return window;
}

void Workstation::closeFile(const QString &absolutePath) {
  WorkstationWindow *window = windowForPath(absolutePath);

  if (!window)
    return;

  onWindowCloseRequested(window);
}

void Workstation::closeAll() {
  if (m_windows.isEmpty()) {
    saveLayout();
    return;
  }

  const QList<WorkstationWindow *> snapshot = m_windows;

  m_windows.clear();
  m_byPath.clear();
  m_focusedWindow = nullptr;
  m_dropTarget = nullptr;

  for (WorkstationWindow *w : snapshot) {
    if (!w)
      continue;

    unwatchFile(w->filePath());

    w->hide();
    w->setParent(nullptr);
    w->deleteLater();
  }

  // Force deferred deletions to run now so the next layout load starts
  // from a clean slate, and so saveLayout below cannot race a pending
  // open.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

  saveLayout();

  emit windowListChanged();
}

void Workstation::refreshAlsoOpenBadges() { updateAlsoOpenBadges(); }

void Workstation::updateAlsoOpenBadges() {
  QHash<QString, int> counts;

  for (WorkstationWindow *w : std::as_const(m_windows)) {
    if (w)
      counts[w->filePath()]++;
  }

  for (WorkstationWindow *w : std::as_const(m_windows)) {
    if (w)
      w->setAlsoOpenElsewhere(counts.value(w->filePath(), 0) > 1);
  }
}

void Workstation::setFileStatus(const QString &absolutePath,
                                const QString &status) {
  WorkstationWindow *window = windowForPath(absolutePath);

  if (window)
    window->setStatus(WorkstationWindow::Status::Neutral, status);
}

void Workstation::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  if (event->oldSize() == event->size())
    return;

  // Deliberately do NOT re-tile on resize. Tiled windows should keep
  // their user-arranged positions. Only floating windows are clamped
  // back into the canvas if they escape.

  constexpr int minVisible = 60;

  for (WorkstationWindow *w : std::as_const(m_windows)) {
    if (!w || w->mode() != WorkstationWindow::Mode::Floating)
      continue;

    QRect g = w->geometry();

    if (g.left() + minVisible > width())
      g.moveLeft(width() - minVisible);

    if (g.top() > height() - minVisible)
      g.moveTop(height() - minVisible);

    if (g.left() < -(g.width() - minVisible))
      g.moveLeft(-(g.width() - minVisible));

    if (g.top() < 0)
      g.moveTop(0);

    w->setGeometry(g);
  }
}

void Workstation::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const ThemeTokens tokens =
      ThemeRegistry::instance().tokens(ThemeRegistry::instance().activeTheme());

  const QColor bg = tokens.base.darker(108);

  painter.fillRect(rect(), bg);

  constexpr int spacing = 40;

  QColor line = tokens.overlay0;
  line.setAlpha(28);

  painter.setPen(QPen(line, 1));

  for (int x = 0; x < width(); x += spacing)
    painter.drawLine(x, 0, x, height());

  for (int y = 0; y < height(); y += spacing)
    painter.drawLine(0, y, width(), y);
}

void Workstation::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);

  QAction *tileAll = menu.addAction(tr("Tile all"));
  QAction *floatAll = menu.addAction(tr("Float all"));
  QAction *arrange = menu.addAction(tr("Arrange all"));
  menu.addSeparator();
  QAction *closeAllAct = menu.addAction(tr("Close all windows"));

  QAction *chosen = menu.exec(event->globalPos());

  if (chosen == tileAll) {
    setAllTiled();
  } else if (chosen == floatAll) {
    setAllFloating();
  } else if (chosen == arrange) {
    arrangeAll();
  } else if (chosen == closeAllAct) {
    closeAll();
  }
}

void Workstation::resetLayout() {
  if (m_windows.isEmpty())
    return;

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Reset layout"),
      tr("Close all windows on the Workstation?"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes)
    return;

  closeAll();
}

void Workstation::saveLayout() {
  const QString path = layoutPath();

  if (path.isEmpty())
    return;

  QJsonArray windowsArr;

  for (WorkstationWindow *w : std::as_const(m_windows)) {
    if (!w)
      continue;

    const QString abs = w->filePath();

    const QString rel = m_outputFolder.isEmpty()
                            ? abs
                            : QDir(m_outputFolder).relativeFilePath(abs);

    const QRect g = w->isMaximized() ? w->restoreGeometry() : w->geometry();

    QJsonObject obj;
    obj.insert(QStringLiteral("file"), rel);
    obj.insert(QStringLiteral("x"), g.x());
    obj.insert(QStringLiteral("y"), g.y());
    obj.insert(QStringLiteral("w"), g.width());
    obj.insert(QStringLiteral("h"), g.height());
    obj.insert(QStringLiteral("z"), w->zOrder());
    obj.insert(QStringLiteral("maximized"), w->isMaximized());
    obj.insert(QStringLiteral("mode"),
               w->mode() == WorkstationWindow::Mode::Tiled
                   ? QStringLiteral("tiled")
                   : QStringLiteral("floating"));

    windowsArr.append(obj);
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), LayoutVersion);
  root.insert(QStringLiteral("windows"), windowsArr);

  QSaveFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return;

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

  file.commit();
}

void Workstation::loadLayout() {
  if (!m_windows.isEmpty())
    closeAll();

  const QString path = layoutPath();

  if (path.isEmpty() || !QFileInfo::exists(path))
    return;

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());

  if (!doc.isObject())
    return;

  const QJsonObject root = doc.object();

  const QJsonArray windowsArr =
      root.value(QStringLiteral("windows")).toArray();

  m_loading = true;

  for (const QJsonValue &value : windowsArr) {
    if (!value.isObject())
      continue;

    const QJsonObject obj = value.toObject();

    const QString rel = obj.value(QStringLiteral("file")).toString();

    if (rel.isEmpty())
      continue;

    const QString abs = absolutePathFor(rel);

    if (!QFileInfo::exists(abs))
      continue;

    WorkstationWindow *window = openFile(abs);

    if (!window)
      continue;

    const int x = obj.value(QStringLiteral("x")).toInt(64);
    const int y = obj.value(QStringLiteral("y")).toInt(64);
    const int w = obj.value(QStringLiteral("w")).toInt(480);
    const int h = obj.value(QStringLiteral("h")).toInt(360);
    const int z = obj.value(QStringLiteral("z")).toInt(0);

    const QString modeStr =
        obj.value(QStringLiteral("mode")).toString(QStringLiteral("floating"));

    const auto mode = modeStr == QStringLiteral("tiled")
                          ? WorkstationWindow::Mode::Tiled
                          : WorkstationWindow::Mode::Floating;

    window->setMode(mode);

    const QRect g(x, y, w, h);

    if (mode == WorkstationWindow::Mode::Floating) {
      window->setGeometry(g);
      window->setRestoreGeometry(g);
    }

    if (z > 0) {
      window->setZOrder(z);

      if (z > m_zCounter)
        m_zCounter = z;
    }

    if (obj.value(QStringLiteral("maximized")).toBool() &&
        mode == WorkstationWindow::Mode::Floating)
      window->toggleMaximize();
  }

  m_loading = false;

  QList<WorkstationWindow *> ordered = m_windows;

  std::sort(ordered.begin(), ordered.end(),
            [](WorkstationWindow *a, WorkstationWindow *b) {
              return a->zOrder() < b->zOrder();
            });

  for (WorkstationWindow *w : std::as_const(ordered)) {
    if (w)
      w->raise();
  }

  updateAlsoOpenBadges();
  applyTiling();

  emit windowListChanged();
}