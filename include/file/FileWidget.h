#ifndef EPISTEME_FILEWIDGET_H
#define EPISTEME_FILEWIDGET_H

#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "FileSystemModel.h"
#include "FileSystemView.h"

class QResizeEvent;

class FileWidget : public QWidget {
  Q_OBJECT

public:
  explicit FileWidget(QWidget *parent = nullptr);

public slots:
  void beginEditingPath(const QString &path);
  void setActivePath(const QString &path);
  void setModifiedPaths(const QSet<QString> &paths);
  void setRootPath(const QString &path);
  signals:
    void fileSelected(const QString &path);
  void renameRequested(const QString &oldPath, const QString &newPath);
  void newNoteRequested(const QString &parentPath);
  void newFolderRequested(const QString &parentPath);
  void deleteRequested(const QString &path);
  void convertToMarkdownRequested(const QString &path);
  void convertToTextRequested(const QString &path);
  void convertToDotRequested(const QString &path);
  void convertToPlantUmlRequested(const QString &path);

  // Emitted when the user picks "Add to Overseer session" from the tree's
  // context menu. Contains all selected files (absolute paths).
  void addToOverseerRequested(const QStringList &paths);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void distributeColumnWidths();

  FileSystemModel *fileSystemModel = nullptr;
  FileSystemView *fileSystemView = nullptr;

  QString pendingEditPath;
};

#endif // EPISTEME_FILEWIDGET_H