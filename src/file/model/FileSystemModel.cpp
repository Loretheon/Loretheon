#include "../../../include/file/model/FileSystemModel.h"

#include "Settings.h"

#include <QFileInfo>

FileSystemModel::FileSystemModel(QObject *parent)
    : QIdentityProxyModel(parent), fsModel(new QFileSystemModel(this)) {
  fsModel->setReadOnly(false);

  setSourceModel(fsModel);

  connect(fsModel, &QFileSystemModel::fileRenamed, this,
          &FileSystemModel::fileRenamed);
}

int FileSystemModel::sourceColumnFor(int proxyColumn) {
  return proxyColumn >= ExtensionColumn ? proxyColumn - 1 : proxyColumn;
}

QModelIndex
FileSystemModel::mapToSourceColumn(const QModelIndex &proxyIndex) const {
  if (!proxyIndex.isValid())
    return QModelIndex();

  const QModelIndex sourceParent = mapToSource(proxyIndex.parent());
  return fsModel->index(proxyIndex.row(), sourceColumnFor(proxyIndex.column()),
                        sourceParent);
}

QModelIndex FileSystemModel::nameIndex(const QModelIndex &proxyIndex) const {
  if (!proxyIndex.isValid())
    return QModelIndex();

  const QModelIndex sourceParent = mapToSource(proxyIndex.parent());
  return fsModel->index(proxyIndex.row(), NameColumn, sourceParent);
}

QModelIndex FileSystemModel::setRootPath(const QString &path) {
  return mapFromSource(fsModel->setRootPath(path));
}

QModelIndex FileSystemModel::index(const QString &path) const {
  return mapFromSource(fsModel->index(path));
}

QString FileSystemModel::filePath(const QModelIndex &index) const {
  return fsModel->filePath(mapToSource(index));
}

bool FileSystemModel::isDir(const QModelIndex &index) const {
  return fsModel->isDir(mapToSource(index));
}

int FileSystemModel::columnCount(const QModelIndex &parent) const {
  return fsModel->columnCount(mapToSource(parent)) + 1;
}

QVariant FileSystemModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid())
    return QVariant();

  const QModelIndex nameIdx = nameIndex(index);
  if (!nameIdx.isValid())
    return QVariant();

  if (index.column() == ExtensionColumn) {
    if (fsModel->isDir(nameIdx))
      return role == Qt::DisplayRole ? QVariant(QString()) : QVariant();

    if (role == Qt::DisplayRole)
      return QFileInfo(fsModel->filePath(nameIdx)).suffix();

    return QVariant();
  }

  if (index.column() == NameColumn &&
      (role == Qt::DisplayRole || role == Qt::EditRole)) {
    if (fsModel->isDir(nameIdx))
      return fsModel->fileName(nameIdx);

    return QFileInfo(fsModel->filePath(nameIdx)).completeBaseName();
  }

  return fsModel->data(mapToSourceColumn(index), role);
}

bool FileSystemModel::setData(const QModelIndex &index, const QVariant &value,
                              int role) {
  if (!index.isValid() || index.column() != NameColumn || role != Qt::EditRole)
    return false;

  const QModelIndex nameIdx = nameIndex(index);
  if (!nameIdx.isValid())
    return false;

  const QFileInfo info(fsModel->filePath(nameIdx));
  const QString extension = info.suffix();
  const QString newBaseName = value.toString();
  const QString newFileName =
      extension.isEmpty() ? newBaseName : newBaseName + '.' + extension;

  return fsModel->setData(nameIdx, newFileName, Qt::EditRole);
}

Qt::ItemFlags FileSystemModel::flags(const QModelIndex &index) const {
  if (!index.isValid())
    return Qt::NoItemFlags;

  if (index.column() == ExtensionColumn)
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;

  Qt::ItemFlags result = fsModel->flags(mapToSourceColumn(index));
  if (index.column() == NameColumn)
    result |= Qt::ItemIsEditable;
  return result;
}

QVariant FileSystemModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    if (section == ExtensionColumn)
      return tr("Extension");
    if (section == NameColumn)
      return tr("Name");
  }

  return fsModel->headerData(sourceColumnFor(section), orientation, role);
}