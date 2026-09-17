#include "../../../include/file/model/FileSystemModel.h"

#include "Settings.h"

#include <QDateTime>
#include <QFont>
#include <QLocale>

FileSystemModel::FileSystemModel(QObject *parent)
    : QIdentityProxyModel(parent), fsModel(new QFileSystemModel(this)) {
  fsModel->setReadOnly(false);

  QIdentityProxyModel::setSourceModel(fsModel);

  connect(fsModel, &QFileSystemModel::fileRenamed, this,
          &FileSystemModel::fileRenamed);
  connect(fsModel, &QFileSystemModel::directoryLoaded, this,
          &FileSystemModel::directoryLoaded);

  connect(fsModel, &QFileSystemModel::dataChanged, this,
          &FileSystemModel::onSourceDataChanged);

  connect(&watcher, &QFileSystemWatcher::fileChanged, this,
          &FileSystemModel::onWatchedFileChanged);
}

void FileSystemModel::setFilter(QDir::Filters filters) {
  fsModel->setFilter(filters);
}

void FileSystemModel::setNameFilters(const QStringList &filters) {
  fsModel->setNameFilters(filters);
}

void FileSystemModel::setNameFilterDisables(bool disable) {
  fsModel->setNameFilterDisables(disable);
}

void FileSystemModel::setReadOnly(bool readOnly) {
  fsModel->setReadOnly(readOnly);
}

void FileSystemModel::watchFile(const QString &path) {
  if (path.isEmpty())
    return;
  if (!QFileInfo::exists(path))
    return;
  if (watcher.files().contains(path))
    return;
  watcher.addPath(path);
}

void FileSystemModel::unwatchFile(const QString &path) {
  if (path.isEmpty())
    return;
  if (watcher.files().contains(path))
    watcher.removePath(path);
}

void FileSystemModel::setActivePath(const QString &path) {
  if (activePath == path)
    return;

  const QString previous = activePath;
  activePath = path;

  emitRowChanged(previous);
  emitRowChanged(activePath);
}

void FileSystemModel::setModifiedPaths(const QSet<QString> &paths) {
  if (modifiedPaths == paths)
    return;

  const QSet<QString> previous = modifiedPaths;
  modifiedPaths = paths;

  for (const QString &path : previous)
    emitRowChanged(path);
  for (const QString &path : modifiedPaths)
    emitRowChanged(path);
}

void FileSystemModel::markModified(const QString &path, bool modified) {
  if (path.isEmpty())
    return;

  const bool changed =
      modified ? !modifiedPaths.contains(path)
               : modifiedPaths.contains(path);
  if (!changed)
    return;

  if (modified)
    modifiedPaths.insert(path);
  else
    modifiedPaths.remove(path);

  emitRowChanged(path);
}

int FileSystemModel::sourceColumnFor(int proxyColumn) {
  switch (proxyColumn) {
  case SizeColumn:
    return 1;
  case TypeColumn:
    return 2;
  case DateModifiedColumn:
    return 3;
  default:
    return -1;
  }
}

QModelIndex FileSystemModel::nameIndex(const QModelIndex &proxyIndex) const {
  if (!proxyIndex.isValid())
    return QModelIndex();

  const QModelIndex sourceParent = mapToSource(proxyIndex.parent());
  return fsModel->index(proxyIndex.row(), 0, sourceParent);
}

QModelIndex
FileSystemModel::mapToSourceColumn(const QModelIndex &proxyIndex) const {
  if (!proxyIndex.isValid())
    return QModelIndex();

  const int sourceColumn = sourceColumnFor(proxyIndex.column());
  if (sourceColumn < 0)
    return QModelIndex();

  const QModelIndex sourceParent = mapToSource(proxyIndex.parent());
  return fsModel->index(proxyIndex.row(), sourceColumn, sourceParent);
}

QModelIndex FileSystemModel::toSource(const QModelIndex &proxyIndex) const {
  if (!proxyIndex.isValid())
    return QModelIndex();

  const QModelIndex sourceParent = mapToSource(proxyIndex.parent());
  const int sourceColumn = sourceColumnFor(proxyIndex.column());
  const int col = (sourceColumn >= 0) ? sourceColumn : 0;

  return fsModel->index(proxyIndex.row(), col, sourceParent);
}

QFileInfo FileSystemModel::fileInfo(const QModelIndex &proxyIndex) const {
  const QModelIndex nameIdx = nameIndex(proxyIndex);
  if (!nameIdx.isValid())
    return QFileInfo();
  return QFileInfo(fsModel->filePath(nameIdx));
}

QModelIndex FileSystemModel::setRootPath(const QString &path) {
  return mapFromSource(fsModel->setRootPath(path));
}

QModelIndex FileSystemModel::index(int row, int column,
                                    const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent))
    return QModelIndex();

  const QModelIndex nameIdx =
      QIdentityProxyModel::index(row, NameColumn, parent);
  if (!nameIdx.isValid())
    return QModelIndex();

  if (column == NameColumn)
    return nameIdx;

  return createIndex(row, column, nameIdx.internalPointer());
}

QModelIndex FileSystemModel::index(const QString &path) const {
  return mapFromSource(fsModel->index(path));
}

QString FileSystemModel::filePath(const QModelIndex &index) const {
  const QModelIndex nameIdx = nameIndex(index);
  if (!nameIdx.isValid())
    return QString();
  return fsModel->filePath(nameIdx);
}

bool FileSystemModel::isDir(const QModelIndex &index) const {
  const QModelIndex nameIdx = nameIndex(index);
  if (!nameIdx.isValid())
    return false;
  return fsModel->isDir(nameIdx);
}

int FileSystemModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return DateCreatedColumn + 1;
}

QVariant FileSystemModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid())
    return QVariant();

  const QModelIndex nameIdx = nameIndex(index);
  if (!nameIdx.isValid())
    return QVariant();

  const QFileInfo info = fileInfo(index);
  const QString path = info.absoluteFilePath();
  const bool isActive = !activePath.isEmpty() && path == activePath;
  const bool isModified = modifiedPaths.contains(path);

  if (role == Qt::FontRole && index.column() == NameColumn) {
    QFont font;
    font.setBold(isActive);
    font.setItalic(isModified);
    return font;
  }

  if (role == Qt::ForegroundRole && index.column() == NameColumn) {
    if (isModified)
      return QColor(Qt::darkYellow);
    if (isActive)
      return QColor(Qt::darkCyan);
    return QVariant();
  }

  if (role == Qt::BackgroundRole && index.column() == NameColumn) {
    if (isActive)
      return QColor(0, 0, 0, 30);
    return QVariant();
  }

  if (role == Qt::ToolTipRole && index.column() == NameColumn) {
    QStringList bits;
    if (isActive)
      bits << tr("Currently open");
    if (isModified)
      bits << tr("Unsaved changes");
    if (!bits.isEmpty())
      return bits.join(QStringLiteral(" — "));
    return QVariant();
  }

  switch (index.column()) {

  case NameColumn:
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
      if (info.isDir())
        return info.fileName();
      return info.completeBaseName();
    }
    return fsModel->data(nameIdx, role);

  case ExtensionColumn:
    if (role == Qt::DisplayRole)
      return info.isDir() ? QVariant() : QVariant(info.suffix());
    return QVariant();

  case SizeColumn:
    if (role == Qt::DisplayRole) {
      if (info.isDir())
        return QVariant();
      const qint64 bytes = info.size();
      if (bytes < 1024)
        return tr("%1 B").arg(bytes);
      return QLocale().formattedDataSize(bytes, 1,
                                         QLocale::DataSizeTraditionalFormat);
    }
    break;

  case TypeColumn:
    if (role == Qt::DisplayRole) {
      if (info.isDir())
        return tr("Folder");
      const QString suffix = info.suffix();
      if (suffix.isEmpty())
        return tr("File");
      return tr("%1 File").arg(suffix.toUpper());
    }
    break;

  case DateModifiedColumn:
    if (role == Qt::DisplayRole) {
      const QDateTime modified = info.lastModified();
      if (!modified.isValid())
        return QVariant();
      return modified.toString(Qt::TextDate);
    }
    if (role == Qt::TextAlignmentRole)
      return int(Qt::AlignLeft | Qt::AlignVCenter);
    return QVariant();

  case DateCreatedColumn:
    if (role == Qt::DisplayRole) {
      const QDateTime created = info.birthTime();
      return created.toString(Qt::TextDate);
    }
    if (role == Qt::TextAlignmentRole)
      return int(Qt::AlignLeft | Qt::AlignVCenter);
    return QVariant();

  default:
    break;
  }

  const QModelIndex sourceIndex = mapToSourceColumn(index);
  if (sourceIndex.isValid())
    return fsModel->data(sourceIndex, role);

  return QVariant();
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
  const QString newBaseName = value.toString().trimmed();
  if (newBaseName.isEmpty())
    return false;

  QString newFileName = newBaseName;
  if (!extension.isEmpty() &&
      !newBaseName.endsWith(QLatin1Char('.') + extension,
                            Qt::CaseInsensitive)) {
    newFileName = newBaseName + QLatin1Char('.') + extension;
  }

  return fsModel->setData(nameIdx, newFileName, Qt::EditRole);
}

Qt::ItemFlags FileSystemModel::flags(const QModelIndex &index) const {
  if (!index.isValid())
    return Qt::NoItemFlags;

  const QModelIndex baseIndex =
      index.column() == NameColumn
          ? index
          : QIdentityProxyModel::index(index.row(), NameColumn, index.parent());

  Qt::ItemFlags result = QIdentityProxyModel::flags(baseIndex);

  if (index.column() != NameColumn)
    result &= ~Qt::ItemIsEditable;

  return result;
}

QVariant FileSystemModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case NameColumn:
      return tr("Name");
    case ExtensionColumn:
      return tr("Extension");
    case SizeColumn:
      return tr("Size");
    case TypeColumn:
      return tr("Type");
    case DateModifiedColumn:
      return tr("Date Modified");
    case DateCreatedColumn:
      return tr("Date Created");
    default:
      return QVariant();
    }
  }

  return QIdentityProxyModel::headerData(section, orientation, role);
}

void FileSystemModel::sort(int column, Qt::SortOrder order) {
  const int sourceColumn = sourceColumnFor(column);
  if (sourceColumn >= 0) {
    fsModel->sort(sourceColumn, order);
    return;
  }

  if (column == NameColumn) {
    fsModel->sort(0, order);
    return;
  }
}

void FileSystemModel::emitVirtualColumnsChanged(
    const QModelIndex &proxyNameIndex) {
  if (!proxyNameIndex.isValid())
    return;

  const int row = proxyNameIndex.row();
  const QModelIndex parent = proxyNameIndex.parent();

  const QModelIndex left = index(row, DateModifiedColumn, parent);
  const QModelIndex right = index(row, DateCreatedColumn, parent);
  if (left.isValid() && right.isValid())
    emit dataChanged(left, right, {Qt::DisplayRole});
}

void FileSystemModel::emitRowChanged(const QString &path) {
  if (path.isEmpty())
    return;

  const QModelIndex sourceName = fsModel->index(path);
  if (!sourceName.isValid())
    return;

  const QModelIndex proxyName = mapFromSource(sourceName);
  if (!proxyName.isValid())
    return;

  const QModelIndex left =
      index(proxyName.row(), NameColumn, proxyName.parent());
  const QModelIndex right =
      index(proxyName.row(), DateCreatedColumn, proxyName.parent());
  if (left.isValid() && right.isValid()) {
    emit dataChanged(left, right,
                     {Qt::DisplayRole, Qt::FontRole, Qt::ForegroundRole,
                      Qt::BackgroundRole, Qt::ToolTipRole});
  }
}

void FileSystemModel::onSourceDataChanged(const QModelIndex &topLeft,
                                          const QModelIndex &bottomRight,
                                          const QVector<int> &roles) {
  const QModelIndex proxyTopLeft = mapFromSource(topLeft);
  const QModelIndex proxyBottomRight = mapFromSource(bottomRight);
  if (proxyTopLeft.isValid() && proxyBottomRight.isValid())
    emit dataChanged(proxyTopLeft, proxyBottomRight, roles);

  for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
    const QModelIndex sourceName =
        fsModel->index(row, 0, topLeft.parent());
    const QModelIndex proxyName = mapFromSource(sourceName);
    emitVirtualColumnsChanged(proxyName);
  }
}

void FileSystemModel::onWatchedFileChanged(const QString &path) {
  const QModelIndex sourceName = fsModel->index(path);
  if (!sourceName.isValid())
    return;

  const QModelIndex proxyName = mapFromSource(sourceName);
  emitVirtualColumnsChanged(proxyName);

  if (QFileInfo::exists(path) && !watcher.files().contains(path))
    watcher.addPath(path);
}