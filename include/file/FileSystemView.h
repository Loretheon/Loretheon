#ifndef EPISTEME_FILESYSTEMVIEW_H
#define EPISTEME_FILESYSTEMVIEW_H

#include <QTreeView>

class FileSystemView : public QTreeView {
  Q_OBJECT

public:
  explicit FileSystemView(QWidget *parent = nullptr);

signals:
  void renameFinished(const QString &oldPath, const QString &newPath);

protected:
  void currentChanged(const QModelIndex &current,
                      const QModelIndex &previous) override;
  void closeEditor(QWidget *editor,
                   QAbstractItemDelegate::EndEditHint hint) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  QString editingOldPath;
};

#endif // EPISTEME_FILESYSTEMVIEW_H