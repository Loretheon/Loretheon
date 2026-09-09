#include "../../include/file/FileSystemView.h"

#include <QContextMenuEvent>
#include <QFileSystemModel>
#include <QLineEdit>
#include <QMenu>

FileSystemView::FileSystemView(QWidget *parent) : QTreeView(parent) {
  setEditTriggers(QAbstractItemView::EditKeyPressed |
                  QAbstractItemView::SelectedClicked);
}

void FileSystemView::currentChanged(const QModelIndex &current,
                                    const QModelIndex &previous) {
  QTreeView::currentChanged(current, previous);

  if (auto *fsModel = qobject_cast<QFileSystemModel *>(model()))
    editingOldPath = fsModel->filePath(current);
}

void FileSystemView::closeEditor(QWidget *editor,
                                 QAbstractItemDelegate::EndEditHint hint) {
  auto *fsModel = qobject_cast<QFileSystemModel *>(model());
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

void FileSystemView::contextMenuEvent(QContextMenuEvent *event) {
  const QModelIndex index = indexAt(event->pos());

  if (!index.isValid())
    return;

  setCurrentIndex(index);

  QMenu menu(this);
  QAction *renameAction = menu.addAction(tr("Rename"));

  QAction *chosen = menu.exec(event->globalPos());

  if (chosen == renameAction)
    edit(index);
}