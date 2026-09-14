#include "../../include/file/FileSystemView.h"

#include "../../include/file/DirectoryExplorerSettings.h"
#include "../../include/file/model/FileSystemModel.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QUrl>

FileSystemView::FileSystemView(QWidget *parent) : QTreeView(parent) {
  setEditTriggers(QAbstractItemView::EditKeyPressed |
                  QAbstractItemView::SelectedClicked);
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
}

void FileSystemView::showColumn(int column) {
  QTreeView::showColumn(column);
  saveColumnVisibility();
}

void FileSystemView::saveColumnVisibility() {
  QList<bool> visibility;
  for (int col = 0; col < 6; ++col)
    visibility << !isColumnHidden(col);
  DirectoryExplorerSettings::instance().setColumnVisibility(visibility);
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
  } else if (chosen == copyPathAction) {
    QApplication::clipboard()->setText(path);
  } else if (chosen == revealAction) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(isDir ? path : QFileInfo(path).dir().path()));
  }
}