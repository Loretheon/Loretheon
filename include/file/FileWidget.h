// FileWidget.h
#ifndef EPISTEME_FILEWIDGET_H
#define EPISTEME_FILEWIDGET_H

#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "FileSystemModel.h"
#include "FileSystemView.h"

class FileWidget : public QWidget {
  Q_OBJECT

public:
  explicit FileWidget(QWidget *parent = nullptr);

  FileSystemView *view() const { return fileSystemView; }

  void setImportableExtensions(const QStringList &extensions);

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
  void convertToMermaidRequested(const QString &path);

  void addToOverseerRequested(const QStringList &paths);

  void importRequested(const QString &path);

  void importAllRequested(const QStringList &paths);

  void promoteToNotesRequested(const QStringList &paths);


private:
  FileSystemModel *fileSystemModel = nullptr;
  FileSystemView *fileSystemView = nullptr;

  QString pendingEditPath;
};

#endif // EPISTEME_FILEWIDGET_H