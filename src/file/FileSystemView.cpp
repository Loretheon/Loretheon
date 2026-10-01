// FileSystemView.cpp
#include "../../include/file/FileSystemView.h"

#include "../../include/file/DirectoryExplorerSettings.h"
#include "../../include/file/model/FileSystemModel.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDrag>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace {

constexpr const char *kNotesPathMimeType =
    "application/x-lore-notes-path";

constexpr int kCellPadding = 12;

constexpr int kIndentationAllowance = 60;

constexpr int kMinimumViewWidth = 260;

const QStringList &defaultImportableExtensions() {
  static const QStringList kExtensions = {
      QStringLiteral("pdf"),  QStringLiteral("html"),
      QStringLiteral("htm"),  QStringLiteral("docx"),
      QStringLiteral("pptx"), QStringLiteral("epub"),
  };
  return kExtensions;
}

} // namespace

const char *FileSystemView::notesPathMimeType() {
  return kNotesPathMimeType;
}

FileSystemView::FileSystemView(QWidget *parent) : QTreeView(parent) {
  setEditTriggers(QAbstractItemView::EditKeyPressed |
                  QAbstractItemView::SelectedClicked);

  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setDragEnabled(true);
  setAcceptDrops(false);
  setDropIndicatorShown(false);
  setDragDropMode(QAbstractItemView::DragOnly);
  setDefaultDropAction(Qt::CopyAction);

  m_importableExtensions = defaultImportableExtensions();

  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setUniformRowHeights(true);
  setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
}

void FileSystemView::setModel(QAbstractItemModel *model) {
  if (QAbstractItemModel *old = QTreeView::model()) {
    disconnect(old, nullptr, this, nullptr);
  }

  QTreeView::setModel(model);
  applyColumnSizing();

  if (!model)
    return;

  connect(model, &QAbstractItemModel::rowsInserted, this,
          [this]() { scheduleColumnWidthRecalculation(); });
  connect(model, &QAbstractItemModel::rowsRemoved, this,
          [this]() { scheduleColumnWidthRecalculation(); });
  connect(model, &QAbstractItemModel::dataChanged, this,
          [this]() { scheduleColumnWidthRecalculation(); });
  connect(model, &QAbstractItemModel::modelReset, this,
          [this]() { scheduleColumnWidthRecalculation(); });
  connect(model, &QAbstractItemModel::layoutChanged, this,
          [this]() { scheduleColumnWidthRecalculation(); });

  if (auto *fsModel = qobject_cast<FileSystemModel *>(model)) {
    connect(fsModel, &FileSystemModel::directoryLoaded, this,
            [this](const QString &) { scheduleColumnWidthRecalculation(); });
  }

  connect(this, &QTreeView::expanded, this,
          [this]() { scheduleColumnWidthRecalculation(); });
  connect(this, &QTreeView::collapsed, this,
          [this]() { scheduleColumnWidthRecalculation(); });

  scheduleColumnWidthRecalculation();
}

void FileSystemView::applyColumnSizing() {
  if (!model())
    return;

  QHeaderView *h = header();
  if (!h)
    return;

  if (m_columnsConfigured)
    return;

  h->setStretchLastSection(false);
  h->setMinimumSectionSize(40);
  h->setSectionsClickable(true);
  h->setSectionsMovable(true);
  h->setSectionResizeMode(QHeaderView::Interactive);

  m_columnsConfigured = true;
}

void FileSystemView::scheduleColumnWidthRecalculation() {
  if (m_recalcScheduled)
    return;

  m_recalcScheduled = true;
  QTimer::singleShot(0, this, [this]() {
    m_recalcScheduled = false;
    recalculateColumnWidths();
  });
}

int FileSystemView::headerWidth(int column) const {
  const QHeaderView *h = header();
  if (!h)
    return 0;
  return h->sectionSizeHint(column);
}

int FileSystemView::contentWidthForRow(int column, const QModelIndex &index,
                                       bool includeChildren) const {
  const QAbstractItemModel *m = model();
  if (!m || !index.isValid())
    return 0;

  QFontMetrics fm(fontMetrics());

  const QString text = m->data(index, Qt::DisplayRole).toString();
  int widest = text.isEmpty() ? 0 : fm.horizontalAdvance(text);

  if (includeChildren && m->hasChildren(index)) {
    const int rows = m->rowCount(index);
    for (int row = 0; row < rows; ++row) {
      const QModelIndex child = m->index(row, column, index);
      widest = std::max(widest,
                        contentWidthForRow(column, child, includeChildren));
    }
  }

  return widest;
}

int FileSystemView::contentWidthRecursive(int column,
                                          const QModelIndex &parent) const {
  const QAbstractItemModel *m = model();
  if (!m)
    return 0;

  int widest = 0;

  const int rows = m->rowCount(parent);
  for (int row = 0; row < rows; ++row) {
    const QModelIndex index = m->index(row, column, parent);
    if (!index.isValid())
      continue;

    widest = std::max(widest,
                      contentWidthForRow(column, index, true));
  }

  return widest;
}

int FileSystemView::contentWidth(int column) const {
  return contentWidthRecursive(column, rootIndex());
}

int FileSystemView::scrollbarAllowance() const {
  return style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 2;
}

int FileSystemView::frameAllowance() const {
  const int frame = frameWidth();
  return frame > 0 ? frame * 2 : 2;
}

int FileSystemView::measuredContentWidth() const {
  if (!model())
    return kMinimumViewWidth;

  const QHeaderView *h = header();
  if (!h)
    return kMinimumViewWidth;

  const int iconPadding =
      (iconSize().width() > 0) ? iconSize().width() + 4 : 0;

  int total = 0;

  for (int col = 0; col < h->count(); ++col) {
    if (isColumnHidden(col))
      continue;

    const int headerW = headerWidth(col);
    const int contentW = contentWidth(col);
    const int extra =
        (col == 0) ? iconPadding + kIndentationAllowance : 0;
    const int target =
        std::max(headerW, contentW + extra) + kCellPadding;

    total += target;
  }

  total += scrollbarAllowance();
  total += frameAllowance();

  return std::max(total, kMinimumViewWidth);
}

void FileSystemView::recalculateColumnWidths() {
  if (!model())
    return;

  QHeaderView *h = header();
  if (!h)
    return;

  const int iconPadding =
      (iconSize().width() > 0) ? iconSize().width() + 4 : 0;

  int total = 0;

  h->blockSignals(true);

  for (int col = 0; col < h->count(); ++col) {
    if (isColumnHidden(col))
      continue;

    const int headerW = headerWidth(col);
    const int contentW = contentWidth(col);
    const int extra =
        (col == 0) ? iconPadding + kIndentationAllowance : 0;
    const int target =
        std::max(headerW, contentW + extra) + kCellPadding;

    h->resizeSection(col, target);
    total += target;
  }

  h->blockSignals(false);

  const int totalWithScrollbar =
      total + scrollbarAllowance() + frameAllowance();

  if (totalWithScrollbar != m_totalContentWidth) {
    m_totalContentWidth = totalWithScrollbar;
    emit preferredContentWidthChanged(m_totalContentWidth);
    updateGeometry();
  }
}

int FileSystemView::preferredContentWidth() const {
  return m_totalContentWidth;
}

QSize FileSystemView::sizeHint() const {
  const int w = std::max(m_totalContentWidth, kMinimumViewWidth);
  return QSize(w, QTreeView::sizeHint().height());
}

QSize FileSystemView::minimumSizeHint() const {
  return QSize(kMinimumViewWidth, QTreeView::minimumSizeHint().height());
}

int FileSystemView::fullContentWidth() const {
  if (!model())
    return kMinimumViewWidth;

  const QHeaderView *h = header();
  if (!h)
    return kMinimumViewWidth;

  const int iconPadding =
      (iconSize().width() > 0) ? iconSize().width() + 4 : 0;

  int total = 0;

  for (int col = 0; col < h->count(); ++col) {
    const int headerW = headerWidth(col);
    const int contentW = contentWidth(col);
    const int extra =
        (col == 0) ? iconPadding + kIndentationAllowance : 0;
    const int target =
        std::max(headerW, contentW + extra) + kCellPadding;

    total += target;
  }

  total += scrollbarAllowance();
  total += frameAllowance();

  return std::max(total, kMinimumViewWidth);
}

void FileSystemView::expandAllAndMeasure() {
  expandAll();
  recalculateColumnWidths();
}

void FileSystemView::showEvent(QShowEvent *event) {
  QTreeView::showEvent(event);

  if (m_firstShowDone)
    return;

  m_firstShowDone = true;

  scheduleColumnWidthRecalculation();
}

void FileSystemView::setImportableExtensions(const QStringList &extensions) {
  QStringList normalized;
  for (const QString &ext : extensions) {
    QString key = ext.toLower();
    if (key.startsWith(QLatin1Char('.'))) {
      key.remove(0, 1);
    }
    if (!key.isEmpty() && !normalized.contains(key)) {
      normalized.append(key);
    }
  }

  m_importableExtensions =
      normalized.isEmpty() ? defaultImportableExtensions() : normalized;
}

QStringList FileSystemView::importableExtensions() const {
  return m_importableExtensions;
}

void FileSystemView::setPromoteToNotesEnabled(bool enabled) {
  m_promoteToNotesEnabled = enabled;
}

bool FileSystemView::isImportablePath(const QString &path) const {
  const QString suffix = QFileInfo(path).suffix().toLower();
  return !suffix.isEmpty() && m_importableExtensions.contains(suffix);
}

void FileSystemView::currentChanged(const QModelIndex &current,
                                    const QModelIndex &previous) {
  QTreeView::currentChanged(current, previous);

  if (auto *fsModel = qobject_cast<FileSystemModel *>(model()))
    editingOldPath = fsModel->filePath(current);
}

void FileSystemView::closeEditor(QWidget *editor,
                                 QAbstractItemDelegate::EndEditHint hint) {
  auto *fsModel = qobject_cast<FileSystemModel *>(model());
  auto *lineEdit = qobject_cast<QLineEdit *>(editor);

  if (fsModel && lineEdit && hint == QAbstractItemDelegate::SubmitModelCache) {
    const QFileInfo info(editingOldPath);
    const QString newPath = info.dir().filePath(lineEdit->text());

    QTreeView::closeEditor(editor, QAbstractItemDelegate::NoHint);

    if (newPath != editingOldPath)
      emit renameFinished(editingOldPath, newPath);

    return;
  }

  QTreeView::closeEditor(editor, hint);
}

void FileSystemView::mouseMoveEvent(QMouseEvent *event) {
  QTreeView::mouseMoveEvent(event);

  // QTreeView with DragOnly leaves the drag cursor on the viewport after
  // a hover, which reads as "the tree is in scroll mode". Restore the
  // arrow cursor whenever no button is held.
  if (event->buttons() == Qt::NoButton) {
    viewport()->setCursor(Qt::ArrowCursor);
  }
}


void FileSystemView::hideColumn(int column) {
  QTreeView::hideColumn(column);
  saveColumnVisibility();
  scheduleColumnWidthRecalculation();
}

void FileSystemView::showColumn(int column) {
  QTreeView::showColumn(column);
  saveColumnVisibility();
  scheduleColumnWidthRecalculation();
}

void FileSystemView::saveColumnVisibility() {
  QList<bool> visibility;
  for (int col = 0; col < 6; ++col)
    visibility << !isColumnHidden(col);
  DirectoryExplorerSettings::instance().setColumnVisibility(visibility);
}

QStringList FileSystemView::selectedPaths(bool includeDirectories) const {
  QStringList paths;

  auto *fsModel = qobject_cast<FileSystemModel *>(model());

  if (!fsModel) {
    return paths;
  }

  const QModelIndexList selected = selectionModel()->selectedRows(0);

  auto append = [&](const QModelIndex &index) {
    if (!index.isValid()) {
      return;
    }

    if (!includeDirectories && fsModel->isDir(index)) {
      return;
    }

    const QString path = fsModel->filePath(index);

    if (path.isEmpty()) {
      return;
    }

    if (!QFileInfo::exists(path)) {
      return;
    }

    if (!paths.contains(path)) {
      paths.append(path);
    }
  };

  if (!selected.isEmpty()) {
    for (const QModelIndex &index : selected) {
      append(index);
    }
  }

  if (paths.isEmpty()) {
    append(currentIndex());
  }

  return paths;
}

QStringList FileSystemView::selectedFilePaths() const {
  return selectedPaths(false);
}

void FileSystemView::startDrag(Qt::DropActions supportedActions) {
  Q_UNUSED(supportedActions);

  const QStringList paths = selectedFilePaths();

  if (paths.isEmpty()) {
    return;
  }

  auto *mime = new QMimeData;
  mime->setData(kNotesPathMimeType,
                paths.join(QChar('\n')).toUtf8());

  mime->setText(paths.join(QChar('\n')));

  auto *drag = new QDrag(this);
  drag->setMimeData(mime);
  drag->exec(Qt::CopyAction, Qt::CopyAction);
}

void FileSystemView::contextMenuEvent(QContextMenuEvent *event) {
  const QModelIndex index = indexAt(event->pos());

  auto *fsModel = qobject_cast<FileSystemModel *>(model());
  if (!fsModel)
    return;

  // Three cases: a click on a file, a click on a directory, and a click
  // on empty space. Empty space means the directory currently shown as
  // the root.

  const bool onRow = index.isValid();
  const bool onDir = onRow && fsModel->isDir(index);
  const bool onFile = onRow && !onDir;
  const bool onEmpty = !onRow;

  if (onRow)
    setCurrentIndex(index);

  const QString path = onRow ? fsModel->filePath(index) : QString();

  const QString parentPath =
      onDir ? path
            : (onFile ? QFileInfo(path).dir().path()
                      : fsModel->filePath(rootIndex()));

  const QString suffix = onFile ? QFileInfo(path).suffix().toLower()
                                : QString();

  const bool isMarkdown = onFile && suffix == QStringLiteral("md");
  const bool isText = onFile && suffix == QStringLiteral("txt");
  const bool isPlantUml =
      onFile && (suffix == QStringLiteral("puml") ||
                 suffix == QStringLiteral("plantuml"));
  const bool isDot = onFile && (suffix == QStringLiteral("dot") ||
                                suffix == QStringLiteral("gv"));
  const bool isMermaid = onFile && (suffix == QStringLiteral("mmd") ||
                                    suffix == QStringLiteral("mermaid"));
  const bool isHtml = onFile && (suffix == QStringLiteral("html") ||
                                 suffix == QStringLiteral("htm"));

  QMenu menu(this);

  QAction *openAction = nullptr;
  if (onFile)
    openAction = menu.addAction(tr("Open"));

  QAction *newNoteAction = nullptr;
  QAction *newFolderAction = nullptr;

  if (onDir || onEmpty) {
    newNoteAction = menu.addAction(tr("New Note"));
    newFolderAction = menu.addAction(tr("New Folder"));
  }

  if (openAction || newNoteAction)
    menu.addSeparator();

  QAction *renameAction = nullptr;
  QAction *deleteAction = nullptr;

  if (onRow) {
    renameAction = menu.addAction(tr("Rename"));
    deleteAction = menu.addAction(tr("Delete"));
    menu.addSeparator();
  }

  QAction *convertToTextAction = nullptr;
  QAction *convertToMarkdownAction = nullptr;
  QAction *convertToPlantUmlAction = nullptr;
  QAction *convertToDotAction = nullptr;
  QAction *convertToMermaidAction = nullptr;

  if (onFile &&
      (isMarkdown || isText || isPlantUml || isDot || isMermaid ||
       isHtml)) {
    if (!isMarkdown)
      convertToMarkdownAction = menu.addAction(tr("Convert to Markdown"));

    if (!isText)
      convertToTextAction = menu.addAction(tr("Convert to Text"));

    if (!isDot)
      convertToDotAction = menu.addAction(tr("Convert to Graphviz"));

    if (!isPlantUml)
      convertToPlantUmlAction = menu.addAction(tr("Convert to PlantUML"));

    if (!isMermaid)
      convertToMermaidAction = menu.addAction(tr("Convert to Mermaid"));

    menu.addSeparator();
  }

  QAction *importAction = nullptr;
  QAction *importAllAction = nullptr;

  if (onFile && isImportablePath(path)) {
    importAction = menu.addAction(tr("Import…"));

    const QStringList selected = selectedFilePaths();
    if (selected.size() > 1) {
      importAllAction = menu.addAction(
          tr("Import All (%1 files)…").arg(selected.size()));
    }

    menu.addSeparator();
  }

  QAction *addToOverseerAction = nullptr;

  if (onFile) {
    addToOverseerAction = menu.addAction(tr("Add to Overseer session"));
    menu.addSeparator();
  }

  QAction *promoteAction = nullptr;

  if (onRow && m_promoteToNotesEnabled) {
    promoteAction = menu.addAction(tr("Promote to notes"));
    menu.addSeparator();
  }

  QAction *copyPathAction = nullptr;
  QAction *revealAction = nullptr;

  if (onRow) {
    copyPathAction = menu.addAction(tr("Copy Path"));
    revealAction = menu.addAction(tr("Show in File Manager"));
  }

  if (menu.actions().isEmpty())
    return;

  QAction *chosen = menu.exec(event->globalPos());
  if (!chosen)
    return;

  if (chosen == openAction) {
    emit openRequested(path);
  } else if (chosen == newNoteAction) {
    emit newNoteRequested(parentPath);
  } else if (chosen == newFolderAction) {
    emit newFolderRequested(parentPath);
  } else if (chosen == renameAction) {
    edit(index);
  } else if (chosen == deleteAction) {
    emit deleteRequested(path);
  } else if (chosen == convertToTextAction) {
    emit convertToTextRequested(path);
  } else if (chosen == convertToMarkdownAction) {
    emit convertToMarkdownRequested(path);
  } else if (chosen == convertToDotAction) {
    emit convertToDotRequested(path);
  } else if (chosen == convertToPlantUmlAction) {
    emit convertToPlantUmlRequested(path);
  } else if (chosen == convertToMermaidAction) {
    emit convertToMermaidRequested(path);
  } else if (chosen == importAction) {
    emit importRequested(path);
  } else if (chosen == importAllAction) {
    QStringList paths;
    for (const QString &candidate : selectedFilePaths()) {
      if (isImportablePath(candidate)) {
        paths.append(candidate);
      }
    }
    if (!paths.isEmpty()) {
      emit importAllRequested(paths);
    }
  } else if (chosen == addToOverseerAction) {
    const QStringList paths = selectedFilePaths();
    if (!paths.isEmpty()) {
      emit addToOverseerRequested(paths);
    }
  } else if (chosen == promoteAction) {
    const QStringList paths = selectedPaths(true);
    if (!paths.isEmpty()) {
      emit promoteToNotesRequested(paths);
    }
  } else if (chosen == copyPathAction) {
    QApplication::clipboard()->setText(path);
  } else if (chosen == revealAction) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(onDir ? path : QFileInfo(path).dir().path()));
  }
}