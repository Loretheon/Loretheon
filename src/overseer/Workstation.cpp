#include "../../include/overseer/Workstation.h"

#include "../../include/app/theme/ThemeTokens.h"
#include "../../include/overseer/WorkstationWindow.h"

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

namespace {

constexpr auto LayoutFilename = "workstation.json";
constexpr int LayoutVersion = 1;

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

void Workstation::onWindowCloseRequested(WorkstationWindow *window) {
  if (!window)
    return;

  const QString path = window->filePath();

  unwatchFile(path);

  m_windows.removeOne(window);
  m_byPath.remove(path);

  if (m_focusedWindow == window)
    m_focusedWindow = nullptr;

  window->hide();
  window->setParent(nullptr);
  window->deleteLater();

  updateAlsoOpenBadges();
  saveLayout();

  emit windowClosed(path);
  emit windowListChanged();
}

void Workstation::onWindowGeometryChanged(WorkstationWindow *window) {
  Q_UNUSED(window);
  saveLayout();
}

void Workstation::onWindowDragFinished(WorkstationWindow *window) {
  if (!window)
    return;

  const QRect resolved = resolveNonOverlappingRect(window->geometry(), window);

  if (resolved != window->geometry()) {
    window->setGeometry(resolved);
    window->setRestoreGeometry(resolved);
  }

  saveLayout();
}

void Workstation::onWindowResizeFinished(WorkstationWindow *window) {
  if (!window)
    return;

  const QRect resolved =
      resolveNonOverlappingResize(window->geometry(), window);

  if (resolved != window->geometry()) {
    window->setGeometry(resolved);
    window->setRestoreGeometry(resolved);
  }

  saveLayout();
}

void Workstation::onDiskFileChanged(const QString &path) {
  // If we wrote the file ourselves, ignore the notification. Qt still
  // fires it for our own writes.
  if (m_ignoreNextChange.contains(path)) {
    m_ignoreNextChange.remove(path);

    // Rewatch; many editors replace the file, which removes the watch.
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
    // Rewatch and bail.
    if (QFileInfo::exists(path) && !m_fileWatcher.files().contains(path))
      m_fileWatcher.addPath(path);
    return;
  }

  if (!doc->isModified()) {
    // No unsaved edits. Reload silently.
    reloadWindowFromDisk(window);
  } else {
    // Unsaved edits present. Show the conflict banner.
    window->showDiskConflictBanner();
  }

  // QFileSystemWatcher removes the path when the file is replaced. Add
  // it back so we keep watching.
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
  window->refreshModifiedIndicator();
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
  window->refreshModifiedIndicator();
}

void Workstation::onDiskConflictReload(WorkstationWindow *window) {
  reloadWindowFromDisk(window);
}

void Workstation::onDiskConflictKeepMine(WorkstationWindow *window) {
  if (!window)
    return;

  // Leave the in-memory document alone. Hide the banner so the user
  // can keep editing; the modified indicator remains, so they know
  // the file is unsaved.
  window->hideDiskConflictBanner();
  window->refreshModifiedIndicator();
}

void Workstation::onDiskConflictOverwrite(WorkstationWindow *window) {
  saveWindowToDisk(window);
}

QSize Workstation::defaultNewWindowSize() const {
  const int canvasW = qMax(width(), 400);
  const int canvasH = qMax(height(), 300);

  int cols = 3;

  if (canvasW < 700)
    cols = 1;
  else if (canvasW < 1100)
    cols = 2;

  const int usableW = canvasW - kMargin * 2 - kGap * (cols - 1);

  const int cellW = qMax(WorkstationWindow::kMinimumWidth, usableW / cols);
  const int cellH = qMax(WorkstationWindow::kMinimumHeight,
                         static_cast<int>(canvasH * 0.62));

  return QSize(cellW, cellH);
}

bool Workstation::rectOverlapsAny(const QRect &candidate,
                                  WorkstationWindow *ignore) const {
  for (WorkstationWindow *w : std::as_const(m_windows)) {
    if (!w || w == ignore)
      continue;

    if (candidate.intersects(w->geometry().adjusted(-kMinGap, -kMinGap,
                                                    kMinGap, kMinGap))) {
      return true;
    }
  }

  return false;
}

QRect Workstation::findFreeSlot(const QSize &size,
                                WorkstationWindow *ignore) const {
  const int canvasW = width();
  const int canvasH = height();

  const int w = qMin(size.width(), canvasW - kMargin * 2);
  const int h = qMin(size.height(), canvasH - kMargin * 2);

  const int step = kGap + 8;

  QList<QRect> occupied;

  occupied.reserve(m_windows.size());

  for (WorkstationWindow *other : std::as_const(m_windows)) {
    if (!other || other == ignore)
      continue;

    occupied.append(other->geometry().adjusted(-kMinGap, -kMinGap, kMinGap,
                                               kMinGap));
  }

  auto overlaps = [&occupied](const QRect &candidate) {
    for (const QRect &r : occupied) {
      if (candidate.intersects(r))
        return true;
    }
    return false;
  };

  for (int y = kMargin; y + h <= canvasH - kMargin; y += step) {
    for (int x = kMargin; x + w <= canvasW - kMargin; x += step) {
      const QRect candidate(x, y, w, h);

      if (!overlaps(candidate))
        return candidate;
    }
  }

  int lowestBottom = kMargin;

  for (WorkstationWindow *existing : std::as_const(m_windows)) {
    if (!existing || existing == ignore)
      continue;

    lowestBottom = qMax(lowestBottom, existing->geometry().bottom() + kGap);
  }

  if (lowestBottom + h <= canvasH - kMargin) {
    return QRect(kMargin, lowestBottom, w, h);
  }

  const int cols = qMax(1, canvasW / (w + kGap));
  const int cellW = (canvasW - kMargin * 2 - kGap * (cols - 1)) / cols;
  const int cellH = qMax(WorkstationWindow::kMinimumHeight,
                         (canvasH - kMargin * 2 - kGap) / 2);

  const int index = m_windows.size();

  const int col = index % cols;
  const int row = (index / cols) % 2;

  const QRect fallback(kMargin + col * (cellW + kGap),
                       kMargin + row * (cellH + kGap),
                       qMax(WorkstationWindow::kMinimumWidth, cellW),
                       qMax(WorkstationWindow::kMinimumHeight, cellH));

  return fallback;
}

QRect Workstation::resolveNonOverlappingRect(
    const QRect &target, WorkstationWindow *window) const {
  constexpr int minVisible = 60;

  QRect clamped = target;

  const int maxX = width() - minVisible;
  const int maxY = height() - minVisible;

  if (clamped.left() > maxX)
    clamped.moveLeft(maxX);

  if (clamped.top() > maxY)
    clamped.moveTop(maxY);

  if (clamped.left() + minVisible < 0)
    clamped.moveLeft(-(clamped.width() - minVisible));

  if (clamped.top() < 0)
    clamped.moveTop(0);

  if (!rectOverlapsAny(clamped, window))
    return clamped;

  const int step = kMinGap + 6;
  const int maxRadius = qMax(width(), height());

  for (int r = step; r <= maxRadius; r += step) {
    const QPoint deltas[] = {
        {r, 0},  {-r, 0}, {0, r},  {0, -r},
        {r, r},  {-r, r}, {r, -r}, {-r, -r},
    };

    for (const QPoint &delta : deltas) {
      QRect candidate = clamped.translated(delta);

      if (candidate.left() + minVisible < 0)
        continue;

      if (candidate.top() < 0)
        continue;

      if (candidate.left() > width() - minVisible)
        continue;

      if (candidate.top() > height() - minVisible)
        continue;

      if (!rectOverlapsAny(candidate, window))
        return candidate;
    }
  }

  return findFreeSlot(target.size(), window);
}

QRect Workstation::resolveNonOverlappingResize(
    const QRect &target, WorkstationWindow *window) const {
  QRect candidate = target;

  const int minW = WorkstationWindow::kMinimumWidth;
  const int minH = WorkstationWindow::kMinimumHeight;

  if (!rectOverlapsAny(candidate, window))
    return candidate;

  while (candidate.height() > minH && rectOverlapsAny(candidate, window))
    candidate.setHeight(candidate.height() - 8);

  while (candidate.width() > minW && rectOverlapsAny(candidate, window))
    candidate.setWidth(candidate.width() - 8);

  if (!rectOverlapsAny(candidate, window))
    return candidate;

  const QSize minSize(qMax(minW, candidate.width()),
                      qMax(minH, candidate.height()));

  QRect slot = findFreeSlot(minSize, window);

  slot.moveTopLeft(target.topLeft());

  if (rectOverlapsAny(slot, window))
    slot = findFreeSlot(minSize, window);

  return slot;
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

  const QSize preferred = defaultNewWindowSize();
  const QRect slot = findFreeSlot(preferred, window);

  window->setGeometry(slot);
  window->setRestoreGeometry(slot);

  connect(window, &WorkstationWindow::closeRequested, this,
          &Workstation::onWindowCloseRequested);

  connect(window, &WorkstationWindow::focusRequested, this,
          &Workstation::onWindowFocusRequested);

  connect(window, &WorkstationWindow::geometryChanged, this,
          &Workstation::onWindowGeometryChanged);

  connect(window, &WorkstationWindow::dragFinished, this,
          &Workstation::onWindowDragFinished);

  connect(window, &WorkstationWindow::resizeFinished, this,
          &Workstation::onWindowResizeFinished);

  connect(window, &WorkstationWindow::closeAllRequested, this,
          &Workstation::closeAll);

  connect(window, &WorkstationWindow::autoArrangeRequested, this,
          &Workstation::autoArrange);

  connect(window, &WorkstationWindow::tileRequested, this, &Workstation::tile);

  connect(window, &WorkstationWindow::cascadeRequested, this,
          &Workstation::cascade);

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
  saveLayout();

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
  // Defer the actual teardown to the next event-loop turn. This method
  // can be invoked from a signal emitted by one of the windows being
  // torn down (e.g. the "Close all" item in a window's context menu).
  // Deleting that window synchronously leaves the emitting QMenu and
  // the widget's event handler pointing at freed memory.
  QTimer::singleShot(0, this, [this]() {
    if (m_windows.isEmpty())
      return;

    const QList<WorkstationWindow *> snapshot = m_windows;
    m_windows.clear();
    m_byPath.clear();
    m_focusedWindow = nullptr;

    for (WorkstationWindow *w : snapshot) {
      if (!w)
        continue;

      unwatchFile(w->filePath());

      w->hide();
      w->setParent(nullptr);
      w->deleteLater();
    }

    // Flush the deferred deletions now so subsequent code (e.g. the
    // file watcher) doesn't race them.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    saveLayout();

    emit windowListChanged();
  });
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
    window->setStatusPill(status);
}

void Workstation::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  for (WorkstationWindow *w : std::as_const(m_windows)) {
    if (!w)
      continue;

    QRect g = w->geometry();

    if (g.right() > width() - 4)
      g.moveRight(width() - 4);

    if (g.bottom() > height() - 4)
      g.moveBottom(height() - 4);

    if (g.left() < 0)
      g.moveLeft(0);

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

  QAction *tileAct = menu.addAction(tr("Tile windows"));
  QAction *cascadeAct = menu.addAction(tr("Cascade windows"));
  QAction *arrangeAct = menu.addAction(tr("Auto-arrange"));
  menu.addSeparator();
  QAction *closeAllAct = menu.addAction(tr("Close all windows"));
  menu.addSeparator();
  QAction *resetAct = menu.addAction(tr("Reset layout"));

  QAction *chosen = menu.exec(event->globalPos());

  if (chosen == tileAct) {
    tile();
  } else if (chosen == cascadeAct) {
    cascade();
  } else if (chosen == arrangeAct) {
    autoArrange();
  } else if (chosen == closeAllAct) {
    closeAll();
  } else if (chosen == resetAct) {
    resetLayout();
  }
}

void Workstation::autoArrange() {
  if (m_windows.isEmpty())
    return;

  QList<WorkstationWindow *> ordered = m_windows;

  std::sort(ordered.begin(), ordered.end(),
            [](WorkstationWindow *a, WorkstationWindow *b) {
              return a->zOrder() < b->zOrder();
            });

  const QList<WorkstationWindow *> snapshot = m_windows;
  m_windows.clear();

  for (WorkstationWindow *w : ordered) {
    const QSize preferred = defaultNewWindowSize();
    const QRect slot = findFreeSlot(preferred, w);

    w->setGeometry(slot);
    w->setRestoreGeometry(slot);

    m_windows.append(w);
  }

  Q_UNUSED(snapshot);

  saveLayout();
}

void Workstation::tile() {
  const int count = m_windows.size();

  if (count == 0)
    return;

  const int cols = qCeil(qSqrt(static_cast<double>(count)));
  const int rows = qCeil(static_cast<double>(count) / cols);

  const int cellW = (width() - kMargin * 2 - kGap * (cols - 1)) / cols;
  const int cellH = (height() - kMargin * 2 - kGap * (rows - 1)) / rows;

  for (int i = 0; i < count; ++i) {
    const int r = i / cols;
    const int c = i % cols;

    const QRect cell(kMargin + c * (cellW + kGap),
                     kMargin + r * (cellH + kGap), cellW, cellH);

    m_windows[i]->setGeometry(cell);
    m_windows[i]->setRestoreGeometry(cell);
  }

  saveLayout();
}

void Workstation::cascade() {
  const int count = m_windows.size();

  if (count == 0)
    return;

  constexpr int step = 28;

  for (int i = 0; i < count; ++i) {
    const int offset = i * step;

    const QRect g(kMargin + offset, kMargin + offset,
                  qMin(560, width() - offset - kMargin * 2),
                  qMin(420, height() - offset - kMargin * 2));

    m_windows[i]->setGeometry(g);
    m_windows[i]->setRestoreGeometry(g);
    m_windows[i]->raise();
  }

  saveLayout();
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
  const QList<WorkstationWindow *> snapshot = m_windows;
  m_windows.clear();
  m_byPath.clear();
  m_focusedWindow = nullptr;

  for (WorkstationWindow *w : snapshot) {
    if (w)
      w->deleteLater();
  }

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

    const QRect g(x, y, w, h);

    window->setGeometry(g);
    window->setRestoreGeometry(g);

    if (z > 0) {
      window->setZOrder(z);

      if (z > m_zCounter)
        m_zCounter = z;
    }

    if (obj.value(QStringLiteral("maximized")).toBool())
      window->toggleMaximize();
  }

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

  emit windowListChanged();
}