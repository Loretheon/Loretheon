#ifndef EPISTEME_FILESYSTEMMODEL_H
#define EPISTEME_FILESYSTEMMODEL_H

#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QIdentityProxyModel>
#include <QSet>
#include <QString>

class FileSystemModel : public QIdentityProxyModel {
  Q_OBJECT

public:
  enum Column {
    NameColumn = 0,
    ExtensionColumn = 1,
    SizeColumn = 2,
    TypeColumn = 3,
    DateModifiedColumn = 4,
    DateCreatedColumn = 5,
  };

  explicit FileSystemModel(QObject *parent = nullptr);

  QModelIndex setRootPath(const QString &path);

  QModelIndex index(const QString &path) const;

  QModelIndex index(int row, int column,
                    const QModelIndex &parent = QModelIndex()) const override;
  QString filePath(const QModelIndex &index) const;
  bool isDir(const QModelIndex &index) const;

  void setFilter(QDir::Filters filters);
  void setNameFilters(const QStringList &filters);
  void setNameFilterDisables(bool disable);
  void setReadOnly(bool readOnly);

  void watchFile(const QString &path);
  void unwatchFile(const QString &path);

  void setActivePath(const QString &path);
  void setModifiedPaths(const QSet<QString> &paths);
  void markModified(const QString &path, bool modified);

  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

signals:
  void fileRenamed(const QString &path, const QString &oldName,
                   const QString &newName);
  void directoryLoaded(const QString &path);

private slots:
  void onSourceDataChanged(const QModelIndex &topLeft,
                           const QModelIndex &bottomRight,
                           const QVector<int> &roles);
  void onWatchedFileChanged(const QString &path);

private:
  static int sourceColumnFor(int proxyColumn);

  QModelIndex toSource(const QModelIndex &proxyIndex) const;
  QModelIndex mapToSourceColumn(const QModelIndex &proxyIndex) const;
  QModelIndex nameIndex(const QModelIndex &proxyIndex) const;
  QFileInfo fileInfo(const QModelIndex &proxyIndex) const;

  void emitVirtualColumnsChanged(const QModelIndex &proxyNameIndex);
  void emitRowChanged(const QString &path);

  QFileSystemModel *fsModel = nullptr;
  QFileSystemWatcher watcher;

  QString activePath;
  QSet<QString> modifiedPaths;
};

#endif // EPISTEME_FILESYSTEMMODEL_H