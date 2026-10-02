// FileWidget.cpp
#include "../../include/file/FileWidget.h"

#include "../../include/file/DirectoryExplorerSettings.h"
#include "Settings.h"

#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QStringList collectExpandedPaths(FileSystemModel *model,
                                 FileSystemView *view,
                                 const QModelIndex &parent = QModelIndex()) {
  QStringList paths;

  const int rows = model->rowCount(parent);
  for (int row = 0; row < rows; ++row) {
    const QModelIndex index = model->index(row, 0, parent);

    if (view->isExpanded(index)) {
      paths << model->filePath(index);
      paths += collectExpandedPaths(model, view, index);
    }
  }

  return paths;
}

} // namespace

FileWidget::FileWidget(QWidget *parent) : QWidget(parent) {
  auto &settings = DirectoryExplorerSettings::instance();

  fileSystemModel = new FileSystemModel(this);
  fileSystemView = new FileSystemView(this);

  fileSystemView->setModel(fileSystemModel);

  fileSystemModel->setReadOnly(false);
  fileSystemModel->setNameFilterDisables(false);
  fileSystemModel->setNameFilters(settings.nameFilters());

  QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
  if (settings.showHidden())
    filters |= QDir::Hidden | QDir::System;
  fileSystemModel->setFilter(filters);

  fileSystemView->setHeaderHidden(settings.headerHidden());
  fileSystemView->setAlternatingRowColors(settings.alternatingRowColors());
  fileSystemView->setIndentation(settings.indentation());
  fileSystemView->setIconSize(QSize(settings.iconSize(), settings.iconSize()));
  fileSystemView->setSortingEnabled(true);
  fileSystemView->sortByColumn(settings.sortColumn(), settings.sortOrder());

  fileSystemView->header()->setSectionsMovable(true);

  const QList<bool> visibility = settings.columnVisibility();
  for (int col = 0; col < 6 && col < visibility.size(); ++col) {
    fileSystemView->setColumnHidden(col, !visibility.at(col));
  }

  const QList<int> order = settings.columnOrder();
  for (int i = 0; i < order.size() && i < 6; ++i) {
    const int logicalIndex = order.at(i);
    const int visualIndex = fileSystemView->header()->visualIndex(logicalIndex);
    if (visualIndex != i)
      fileSystemView->header()->moveSection(visualIndex, i);
  }

  connect(fileSystemView, &FileSystemView::clicked, this,
          [this](const QModelIndex &index) {
            if (fileSystemModel->isDir(index))
              return;

            emit fileSelected(fileSystemModel->filePath(index));
          });

  connect(fileSystemView, &FileSystemView::openRequested, this,
          &FileWidget::fileSelected);

  connect(fileSystemModel, &FileSystemModel::fileRenamed, this,
          [this](const QString &path, const QString &oldName,
                 const QString &newName) {
            emit renameRequested(QDir(path).filePath(oldName),
                                 QDir(path).filePath(newName));
          });

  connect(fileSystemView, &FileSystemView::renameFinished, this,
          &FileWidget::renameRequested);

  connect(fileSystemModel, &QAbstractItemModel::rowsInserted, this,
          [this](const QModelIndex &parent, int first, int last) {
            if (pendingEditPath.isEmpty())
              return;

            for (int row = first; row <= last; ++row) {
              const QModelIndex index = fileSystemModel->index(row, 0, parent);

              if (fileSystemModel->filePath(index) == pendingEditPath) {
                fileSystemView->setFocus();
                fileSystemView->setCurrentIndex(index);
                fileSystemView->edit(index);
                pendingEditPath.clear();
                break;
              }
            }
          });

  connect(fileSystemView, &FileSystemView::newNoteRequested, this,
          &FileWidget::newNoteRequested);
  connect(fileSystemView, &FileSystemView::newFolderRequested, this,
          &FileWidget::newFolderRequested);
  connect(fileSystemView, &FileSystemView::deleteRequested, this,
          &FileWidget::deleteRequested);

  connect(fileSystemView, &FileSystemView::convertToMarkdownRequested, this,
          &FileWidget::convertToMarkdownRequested);
  connect(fileSystemView, &FileSystemView::convertToTextRequested, this,
          &FileWidget::convertToTextRequested);
  connect(fileSystemView, &FileSystemView::convertToPlantUmlRequested, this,
          &FileWidget::convertToPlantUmlRequested);
  connect(fileSystemView, &FileSystemView::convertToDotRequested, this,
          &FileWidget::convertToDotRequested);
  connect(fileSystemView, &FileSystemView::convertToMermaidRequested, this,
          &FileWidget::convertToMermaidRequested);

  connect(fileSystemView, &FileSystemView::addToOverseerRequested, this,
          &FileWidget::addToOverseerRequested);

  connect(fileSystemView, &FileSystemView::importRequested, this,
          &FileWidget::importRequested);

  connect(fileSystemView, &FileSystemView::importAllRequested, this,
          &FileWidget::importAllRequested);

  auto saveExpanded = [this]() {
    DirectoryExplorerSettings::instance().setExpandedPaths(
        collectExpandedPaths(fileSystemModel, fileSystemView));
  };

  connect(fileSystemView, &QTreeView::expanded, this,
          [saveExpanded](const QModelIndex &) { saveExpanded(); });
  connect(fileSystemView, &QTreeView::collapsed, this,
          [saveExpanded](const QModelIndex &) { saveExpanded(); });

  connect(fileSystemView->header(), &QHeaderView::sortIndicatorChanged, this,
          [this](int column, Qt::SortOrder order) {
            auto &s = DirectoryExplorerSettings::instance();
            s.setSortColumn(column);
            s.setSortOrder(order);
            s.setHeaderState(fileSystemView->header()->saveState());
          });

  connect(fileSystemView->header(), &QHeaderView::sectionMoved, this,
          [this](int, int, int) {
            QList<int> order;
            QHeaderView *header = fileSystemView->header();
            for (int i = 0; i < header->count(); ++i)
              order << header->logicalIndex(i);
            DirectoryExplorerSettings::instance().setColumnOrder(order);
            DirectoryExplorerSettings::instance().setHeaderState(
                header->saveState());
          });

  fileSystemView->header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(fileSystemView->header(), &QHeaderView::customContextMenuRequested,
          this, [this](const QPoint &pos) {
            QMenu menu;
            const struct {
              int col;
              const char *label;
            } cols[] = {
                {FileSystemModel::NameColumn, QT_TR_NOOP("Name")},
                {FileSystemModel::ExtensionColumn, QT_TR_NOOP("Extension")},
                {FileSystemModel::SizeColumn, QT_TR_NOOP("Size")},
                {FileSystemModel::TypeColumn, QT_TR_NOOP("Type")},
                {FileSystemModel::DateModifiedColumn,
                 QT_TR_NOOP("Date Modified")},
                {FileSystemModel::DateCreatedColumn,
                 QT_TR_NOOP("Date Created")},
            };
            for (const auto &c : cols) {
              QAction *a = menu.addAction(tr(c.label));
              a->setCheckable(true);
              a->setChecked(!fileSystemView->isColumnHidden(c.col));
              if (c.col == FileSystemModel::NameColumn)
                a->setEnabled(false);
              connect(a, &QAction::toggled, this,
                      [this, col = c.col](bool on) {
                        fileSystemView->setColumnHidden(col, !on);
                        QList<bool> vis;
                        for (int i = 0; i < 6; ++i)
                          vis << !fileSystemView->isColumnHidden(i);
                        DirectoryExplorerSettings::instance()
                            .setColumnVisibility(vis);
                        DirectoryExplorerSettings::instance().setHeaderState(
                            fileSystemView->header()->saveState());
                      });
            }
            menu.exec(fileSystemView->header()->mapToGlobal(pos));
          });

  connect(fileSystemView->selectionModel(),
          &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex &current, const QModelIndex &) {
            if (current.isValid()) {
              DirectoryExplorerSettings::instance().setSelectedPath(
                  fileSystemModel->filePath(current));
            }
          });

  const QString root = settings.rootDirectory().isEmpty()
                           ? Settings::getRootDirectory()
                           : settings.rootDirectory();

  settings.setRootDirectory(root);

  const QModelIndex rootIndex = fileSystemModel->setRootPath(root);
  fileSystemView->setRootIndex(rootIndex);

  const QStringList expanded = settings.expandedPaths();
  if (!expanded.isEmpty()) {
    auto restoreExpanded = [this, expanded]() {
      for (const QString &path : expanded) {
        const QModelIndex idx = fileSystemModel->index(path);
        if (idx.isValid() && !fileSystemView->isExpanded(idx))
          fileSystemView->expand(idx);
      }
    };

    QTimer::singleShot(0, this, restoreExpanded);

    connect(fileSystemModel, &FileSystemModel::directoryLoaded, this,
            [restoreExpanded](const QString &) { restoreExpanded(); });
  }

  const QString selectedPath = settings.selectedPath();
  if (!selectedPath.isEmpty()) {
    const QModelIndex selectedIndex = fileSystemModel->index(selectedPath);
    if (selectedIndex.isValid()) {
      fileSystemView->setCurrentIndex(selectedIndex);
      fileSystemView->scrollTo(selectedIndex);
    }
  }

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(fileSystemView);
}

void FileWidget::setImportableExtensions(const QStringList &extensions) {
  if (fileSystemView) {
    fileSystemView->setImportableExtensions(extensions);
  }
}

void FileWidget::beginEditingPath(const QString &path) {
  const QModelIndex existing = fileSystemModel->index(path);

  if (existing.isValid()) {
    fileSystemView->setFocus();
    fileSystemView->setCurrentIndex(existing);
    fileSystemView->edit(existing);
    return;
  }

  pendingEditPath = path;
}

void FileWidget::setActivePath(const QString &path) {
  fileSystemModel->setActivePath(path);
}

void FileWidget::setModifiedPaths(const QSet<QString> &paths) {
  fileSystemModel->setModifiedPaths(paths);
}

void FileWidget::setRootPath(const QString &path) {
  if (path.isEmpty() || !QDir(path).exists())
    return;

  const QModelIndex rootIndex = fileSystemModel->setRootPath(path);
  fileSystemView->setRootIndex(rootIndex);
  fileSystemView->clearSelection();
  fileSystemView->setCurrentIndex(QModelIndex());
  fileSystemView->scrollToTop();

  pendingEditPath.clear();

  DirectoryExplorerSettings::instance().setExpandedPaths({});
  DirectoryExplorerSettings::instance().setSelectedPath({});

  QTimer::singleShot(0, fileSystemView,
                     [this]() { fileSystemView->expandAllAndMeasure(); });
}