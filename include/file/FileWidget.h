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

  // The embedded tree view.  Exposed so the hosting splitter can clamp
  // this widget's maximum width to the view's preferred content width.
  FileSystemView *view() const { return fileSystemView; }

  // Forwarded to the view. Lower-case extensions, without the dot, that
  // the ingest layer can convert. Controls the context menu's "Import…"
  // entries.
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

  // Emitted when the user picks "Add to Overseer session" from the tree's
  // context menu. Contains all selected files (absolute paths).
  void addToOverseerRequested(const QStringList &paths);

  // Emitted when the user picks "Import…" from the tree's context menu.
  // Carries one source file a registered extractor can handle.
  void importRequested(const QString &path);

  // Emitted when the user picks "Import All…" from the tree's context
  // menu. Carries every selected source file an extractor can handle.
  void importAllRequested(const QStringList &paths);

  // Emitted when the user picks "Promote to notes" from the tree's
  // context menu. Contains the absolute paths of every selected file
  // or folder.
  void promoteToNotesRequested(const QStringList &paths);

private:
  FileSystemModel *fileSystemModel = nullptr;
  FileSystemView *fileSystemView = nullptr;

  QString pendingEditPath;
};

#endif // EPISTEME_FILEWIDGET_H