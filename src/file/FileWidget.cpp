#include "../../include/file/FileWidget.h"

#include <QVBoxLayout>

#include "Settings.h"
#include <QDir>

FileWidget::FileWidget(QWidget *parent) : QWidget(parent) {
  fileSystemModel = new FileSystemModel(this);
  fileSystemView = new FileSystemView(this);

  fileSystemView->setModel(fileSystemModel);

  connect(fileSystemView, &FileSystemView::doubleClicked, this,
          [this](const QModelIndex &index) {
            if (fileSystemModel->isDir(index))
              return;

            emit fileSelected(fileSystemModel->filePath(index));
          });

  connect(fileSystemModel, &FileSystemModel::fileRenamed, this,
          [this](const QString &path, const QString &oldName,
                 const QString &newName) {
            emit renameRequested(QDir(path).filePath(oldName),
                                 QDir(path).filePath(newName));
          });
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

  const QString root = Settings::getRootDirectory();

  const QModelIndex rootIndex = fileSystemModel->setRootPath(root);
  fileSystemView->setRootIndex(rootIndex);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(fileSystemView);
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