#include "../../../include/file/model/FileSystemModel.h"

#include "Settings.h"

#include <QFileInfo>

FileSystemModel::FileSystemModel(QObject *parent)
    : QIdentityProxyModel(parent)
{
    fsModel = new QFileSystemModel(this);
    fsModel->setReadOnly(false);

    setSourceModel(fsModel);
}

QModelIndex FileSystemModel::setRootPath(const QString &path)
{
    const QModelIndex sourceRoot = fsModel->setRootPath(path);
    return mapFromSource(sourceRoot);
}

QModelIndex FileSystemModel::index(const QString &path) const
{
    return mapFromSource(fsModel->index(path));
}

QString FileSystemModel::filePath(const QModelIndex &index) const
{
    return fsModel->filePath(mapToSource(index));
}

bool FileSystemModel::isDir(const QModelIndex &index) const
{
    return fsModel->isDir(mapToSource(index));
}

int FileSystemModel::columnCount(const QModelIndex &parent) const
{
    return fsModel->columnCount(mapToSource(parent)) + 1;
}

QModelIndex FileSystemModel::mapToSourceColumn(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid())
        return QModelIndex();

    const int column = proxyIndex.column();
    const int sourceColumn = column >= ExtensionColumn ? column - 1 : column;

    const QModelIndex sourceParent = mapToSource(proxyIndex.parent());
    return fsModel->index(proxyIndex.row(), sourceColumn, sourceParent);
}

QVariant FileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.column() == ExtensionColumn)
    {
        const QModelIndex nameIndex = fsModel->index(index.row(), 0, mapToSource(index.parent()));

        if (fsModel->isDir(nameIndex))
            return (role == Qt::DisplayRole) ? QVariant(QString()) : QVariant();

        if (role == Qt::DisplayRole)
        {
            const QFileInfo info(fsModel->filePath(nameIndex));
            return info.suffix();
        }

        return QVariant();
    }

    if (index.column() == NameColumn && (role == Qt::DisplayRole || role == Qt::EditRole))
    {
        const QModelIndex nameIndex = fsModel->index(index.row(), 0, mapToSource(index.parent()));

        if (fsModel->isDir(nameIndex))
            return fsModel->fileName(nameIndex);

        const QFileInfo info(fsModel->filePath(nameIndex));
        return info.completeBaseName();
    }

    return QIdentityProxyModel::data(mapToSourceColumn(index), role);
}

bool FileSystemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.column() != NameColumn || role != Qt::EditRole)
        return false;

    const QModelIndex nameIndex = fsModel->index(index.row(), 0, mapToSource(index.parent()));
    const QFileInfo info(fsModel->filePath(nameIndex));

    const QString extension = info.suffix();
    const QString newBaseName = value.toString();

    const QString newFileName = extension.isEmpty()
        ? newBaseName
        : newBaseName + "." + extension;

    return fsModel->setData(nameIndex, newFileName, Qt::EditRole);
}

Qt::ItemFlags FileSystemModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    if (index.column() == ExtensionColumn)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    if (index.column() == NameColumn)
        return QIdentityProxyModel::flags(mapToSourceColumn(index)) | Qt::ItemIsEditable;

    return QIdentityProxyModel::flags(mapToSourceColumn(index));
}

QVariant FileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (section == ExtensionColumn)
            return tr("Extension");

        if (section == NameColumn)
            return tr("Name");
    }

    const int sourceSection = section >= ExtensionColumn ? section - 1 : section;
    return fsModel->headerData(sourceSection, orientation, role);
}