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
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QTimer>
#include <QUrl>

namespace {

constexpr const char *kNotesPathMimeType =
    "application/x-lore-notes-path";

// Padding added to the widest content cell so text does not touch the
// next column's edge.
constexpr int kCellPadding = 8;

// Width reserved for the tree indentation column and the expand/collapse
// arrows at the left of the view.  This is not part of any section but
// it does consume horizontal space.
constexpr int kIndentationAllowance = 40;

// Minimal sensible width for the whole view, so the splitter cannot
// collapse us into nothing.  Also used as the floor when the user has
// hidden most columns.
constexpr int kMinimumViewWidth = 120;

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
  h->setSectionResizeMode(QHeaderView::Fixed);

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

int FileSystemView::contentWidthRecursive(int column,
                                          const QModelIndex &parent) const {
  const QAbstractItemModel *m = model();
  if (!m)
    return 0;

  QFontMetrics fm(fontMetrics());
  int widest = 0;

  const int rows = m->rowCount(parent);
  for (int row = 0; row < rows; ++row) {
    const QModelIndex index = m->index(row, column, parent);
    if (!index.isValid())
      continue;

    const QString text = m->data(index, Qt::DisplayRole).toString();
    if (!text.isEmpty())
      widest = std::max(widest, fm.horizontalAdvance(text));

    if (m->hasChildren(index) && isExpanded(index))
      widest = std::max(widest, contentWidthRecursive(column, index));
  }

  return widest;
}

int FileSystemView::contentWidth(int column) const {
  return contentWidthRecursive(column, rootIndex());
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
    const int extra = (col == 0) ? iconPadding : 0;
    const int target = std::max(headerW, contentW + extra) + kCellPadding;

    h->resizeSection(col, target);
    total += target;
  }

  h->blockSignals(false);

  total += kIndentationAllowance;

  if (total != m_totalContentWidth) {
    m_totalContentWidth = total;
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

  if (!index.isValid())
    return;

  setCurrentIndex(index);

  auto *fsModel = qobject_cast<FileSystemModel *>(model());
  if (!fsModel)
    return;

  const QString path = fsModel->filePath(index);
  const bool isDir = fsModel->isDir(index);
  const QString parentPath =
      isDir ? path : QFileInfo(path).dir().path();

  const QString suffix = QFileInfo(path).suffix().toLower();
  const bool isMarkdown = !isDir && suffix == QStringLiteral("md");
  const bool isText = !isDir && suffix == QStringLiteral("txt");
  const bool isPlantUml = !isDir && (suffix == QStringLiteral("puml") ||
                                      suffix == QStringLiteral("plantuml"));
  const bool isDot = !isDir && (suffix == QStringLiteral("dot") ||
                                 suffix == QStringLiteral("gv"));

  QMenu menu(this);

  QAction *newNoteAction = menu.addAction(tr("New Note"));
  QAction *newFolderAction = menu.addAction(tr("New Folder"));
  menu.addSeparator();

  QAction *renameAction = menu.addAction(tr("Rename"));
  QAction *deleteAction = menu.addAction(tr("Delete"));

  QAction *convertToTextAction = nullptr;
  QAction *convertToMarkdownAction = nullptr;
  QAction *convertToPlantUmlAction = nullptr;
  QAction *convertToDotAction = nullptr;

  if (!isDir && (isMarkdown || isText || isPlantUml || isDot)) {
    menu.addSeparator();

    if (!isMarkdown)
      convertToMarkdownAction = menu.addAction(tr("Convert to Markdown"));

    if (!isText)
      convertToTextAction = menu.addAction(tr("Convert to Text"));

    if (!isDot)
      convertToDotAction = menu.addAction(tr("Convert to Graphviz"));

    if (!isPlantUml)
      convertToPlantUmlAction = menu.addAction(tr("Convert to PlantUML"));
  }

  QAction *importAction = nullptr;
  QAction *importAllAction = nullptr;

  if (!isDir && isImportablePath(path)) {
    menu.addSeparator();
    importAction = menu.addAction(tr("Import…"));

    const QStringList selected = selectedFilePaths();
    if (selected.size() > 1) {
      importAllAction = menu.addAction(
          tr("Import All (%1 files)…").arg(selected.size()));
    }
  }

  menu.addSeparator();

  QAction *addToOverseerAction = nullptr;

  if (!isDir) {
    addToOverseerAction = menu.addAction(tr("Add to Overseer session"));
  }

  menu.addSeparator();

  QAction *promoteAction = menu.addAction(tr("Promote to notes"));

  menu.addSeparator();

  QAction *copyPathAction = menu.addAction(tr("Copy Path"));
  QAction *revealAction = menu.addAction(tr("Show in File Manager"));

  QAction *chosen = menu.exec(event->globalPos());
  if (!chosen)
    return;

  if (chosen == renameAction) {
    edit(index);
  } else if (chosen == newNoteAction) {
    emit newNoteRequested(parentPath);
  } else if (chosen == newFolderAction) {
    emit newFolderRequested(parentPath);
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
        QUrl::fromLocalFile(isDir ? path : QFileInfo(path).dir().path()));
  }
}