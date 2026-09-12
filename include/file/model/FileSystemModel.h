#ifndef EPISTEME_FILESYSTEMMODEL_H
#define EPISTEME_FILESYSTEMMODEL_H

#include <QFileSystemModel>
#include <QIdentityProxyModel>

class FileSystemModel : public QIdentityProxyModel {
  Q_OBJECT

public:
  enum Column { NameColumn = 0, ExtensionColumn = 1 };

  explicit FileSystemModel(QObject *parent = nullptr);

  using QIdentityProxyModel::index;

  QModelIndex setRootPath(const QString &path);
  QModelIndex index(const QString &path) const;
  QString filePath(const QModelIndex &index) const;
  bool isDir(const QModelIndex &index) const;

  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  signals:
    void fileRenamed(const QString &path, const QString &oldName,
                     const QString &newName);

private:
  static int sourceColumnFor(int proxyColumn);

  QModelIndex mapToSourceColumn(const QModelIndex &proxyIndex) const;
  QModelIndex nameIndex(const QModelIndex &proxyIndex) const;

  QFileSystemModel *fsModel = nullptr;
};

#endif // EPISTEME_FILESYSTEMMODEL_H