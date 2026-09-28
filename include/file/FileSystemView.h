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
  // at its current computed width, plus the allowances the viewport
  // itself needs (vertical scrollbar, frame). This is what a hosting
  // splitter should treat as the view's natural width.
  int preferredContentWidth() const;

  // Width the view needs so that no column shows a truncated cell and
  // no horizontal scrollbar is needed. Recomputed from the model's
  // current contents, not from a cached value.
  int measuredContentWidth() const;

  // Expand every directory under the current root and then recompute
  // column widths once. Used to bring the tree to its full natural
  // width the first time it is shown.
  void expandAllAndMeasure();

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  int fullContentWidth() const;
public slots:
  // Recompute all column widths from the model's current contents.
  // Safe to call frequently; it coalesces via a single-shot timer.
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
  void showEvent(QShowEvent *event) override;

private:
  void saveColumnVisibility();
  void applyColumnSizing();
  void recalculateColumnWidths();

  int headerWidth(int column) const;
  int contentWidth(int column) const;
  int contentWidthRecursive(int column, const QModelIndex &parent) const;

  // Width of one row's cell, plus (when includeChildren) every
  // descendant row's cell in the same column. Used so that collapsed
  // subtrees still contribute to the measurement.
  int contentWidthForRow(int column, const QModelIndex &index,
                         bool includeChildren) const;

  // Space the viewport reserves for the vertical scrollbar. Without
  // this, the last column's right edge is exactly flush with the
  // scrollbar's left edge and Qt turns on a horizontal scrollbar.
  int scrollbarAllowance() const;

  // Space the viewport reserves for its own frame.
  int frameAllowance() const;

  QStringList selectedFilePaths() const;
  QStringList selectedPaths(bool includeDirectories) const;

  bool isImportablePath(const QString &path) const;

  QString editingOldPath;
  QStringList m_importableExtensions;
  bool m_columnsConfigured = false;
  bool m_recalcScheduled = false;
  bool m_firstShowDone = false;
  int m_totalContentWidth = 0;
};

#endif // EPISTEME_FILESYSTEMVIEW_H