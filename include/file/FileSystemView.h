// FileSystemView.h
#ifndef EPISTEME_FILESYSTEMVIEW_H
#define EPISTEME_FILESYSTEMVIEW_H

#include <QStringList>
#include <QTreeView>

class QAbstractItemModel;

class FileSystemView : public QTreeView {
  Q_OBJECT

public:
  explicit FileSystemView(QWidget *parent = nullptr);

  static const char *notesPathMimeType();

  void setModel(QAbstractItemModel *model) override;

  void hideColumn(int column);
  void showColumn(int column);

  void setImportableExtensions(const QStringList &extensions);
  QStringList importableExtensions() const;

  // Total width needed to show every currently-visible column in full,
  // at its current computed width.  This is what a hosting splitter
  // should treat as the view's maximum sensible width.
  int preferredContentWidth() const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

public slots:
  // Recompute all column widths from the model's current contents.  Safe
  // to call frequently; it coalesces via a single-shot timer.
  void scheduleColumnWidthRecalculation();

signals:
  // Emitted after the view's preferred content width changes, so a
  // hosting splitter can re-clamp itself.
  void preferredContentWidthChanged(int width);

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
  void applyColumnSizing();
  void recalculateColumnWidths();

  int headerWidth(int column) const;
  int contentWidth(int column) const;
  int contentWidthRecursive(int column, const QModelIndex &parent) const;

  QStringList selectedFilePaths() const;
  QStringList selectedPaths(bool includeDirectories) const;

  bool isImportablePath(const QString &path) const;

  QString editingOldPath;
  QStringList m_importableExtensions;
  bool m_columnsConfigured = false;
  bool m_recalcScheduled = false;
  int m_totalContentWidth = 0;
};

#endif // EPISTEME_FILESYSTEMVIEW_H