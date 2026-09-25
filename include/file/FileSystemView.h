#ifndef EPISTEME_FILESYSTEMVIEW_H
#define EPISTEME_FILESYSTEMVIEW_H

#include <QStringList>
#include <QTreeView>

class FileSystemView : public QTreeView {
  Q_OBJECT

public:
  explicit FileSystemView(QWidget *parent = nullptr);

  static const char *notesPathMimeType();

  void hideColumn(int column);
  void showColumn(int column);

  void setImportableExtensions(const QStringList &extensions);
  QStringList importableExtensions() const;

signals:
  void renameFinished(const QString &oldPath, const QString &newPath);
  void newNoteRequested(const QString &parentPath);
  void newFolderRequested(const QString &parentPath);
  void deleteRequested(const QString &path);
  void convertToMarkdownRequested(const QString &path);
  void convertToTextRequested(const QString &path);
  void convertToDotRequested(const QString &path);
  void convertToPlantUmlRequested(const QString &path);

  void addToOverseerRequested(const QStringList &paths);

  void importRequested(const QString &path);
  void importAllRequested(const QStringList &paths);

  // Emitted when the user picks "Promote to notes" from the context
  // menu. Contains the absolute paths of every selected file or folder.
  void promoteToNotesRequested(const QStringList &paths);

protected:
  void currentChanged(const QModelIndex &current,
                      const QModelIndex &previous) override;
  void closeEditor(QWidget *editor,
                   QAbstractItemDelegate::EndEditHint hint) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void startDrag(Qt::DropActions supportedActions) override;

private:
  void saveColumnVisibility();

  // Selected files, in view order. Directories are included when the
  // caller asks for them, because promotion accepts folders as well as
  // files.
  QStringList selectedFilePaths() const;
  QStringList selectedPaths(bool includeDirectories) const;

  bool isImportablePath(const QString &path) const;

  QString editingOldPath;
  QStringList m_importableExtensions;
};

#endif // EPISTEME_FILESYSTEMVIEW_H