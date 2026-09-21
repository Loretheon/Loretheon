#ifndef EPISTEME_FILESYSTEMVIEW_H
#define EPISTEME_FILESYSTEMVIEW_H

#include <QStringList>
#include <QTreeView>

class FileSystemView : public QTreeView {
  Q_OBJECT

public:
  explicit FileSystemView(QWidget *parent = nullptr);

  // The MIME type used when dragging notes from this view. Both the drag
  // source here and the drop target in OverseerOverviewEditor read/write
  // this type. Defined in FileSystemView.cpp.
  static const char *notesPathMimeType();

  void hideColumn(int column);
  void showColumn(int column);

  // Lower-case extensions, without the dot, that the ingest layer can
  // convert. Controls whether the context menu offers "Import…".
  // Defaults to the six built-in formats; MainWindow overrides it from
  // the live IngestRegistry so new extractors appear automatically.
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

  // Emitted when the user picks "Add to Overseer session" from the context
  // menu. Contains all selected files, in view order. Directories are
  // excluded.
  void addToOverseerRequested(const QStringList &paths);

  // Emitted when the user picks "Import…" from the context menu. Carries
  // a single source file that a registered extractor can handle.
  void importRequested(const QString &path);

  // Emitted when the user picks "Import All…" from the context menu.
  // Carries every selected file a registered extractor can handle.
  void importAllRequested(const QStringList &paths);

protected:
  void currentChanged(const QModelIndex &current,
                      const QModelIndex &previous) override;
  void closeEditor(QWidget *editor,
                   QAbstractItemDelegate::EndEditHint hint) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void startDrag(Qt::DropActions supportedActions) override;

private:
  void saveColumnVisibility();

  // Collects the currently selected, existing, non-directory files, in
  // view order. If nothing is selected but the cursor is on a file, that
  // file is returned as a single-element list.
  QStringList selectedFilePaths() const;

  bool isImportablePath(const QString &path) const;

  QString editingOldPath;
  QStringList m_importableExtensions;
};

#endif // EPISTEME_FILESYSTEMVIEW_H