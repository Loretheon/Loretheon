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

  void setPromoteToNotesEnabled(bool enabled);
  bool promoteToNotesEnabled() const { return m_promoteToNotesEnabled; }

  int preferredContentWidth() const;
  int measuredContentWidth() const;
  void expandAllAndMeasure();

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  int fullContentWidth() const;

public slots:
  void scheduleColumnWidthRecalculation();

signals:
  void preferredContentWidthChanged(int width);

  void renameFinished(const QString &oldPath, const QString &newPath);
  void openRequested(const QString &path);

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

protected:
  void currentChanged(const QModelIndex &current,
                      const QModelIndex &previous) override;
  void closeEditor(QWidget *editor,
                   QAbstractItemDelegate::EndEditHint hint) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void startDrag(Qt::DropActions supportedActions) override;
  void showEvent(QShowEvent *event) override;

private:
  void saveColumnVisibility();
  void applyColumnSizing();
  void recalculateColumnWidths();

  int headerWidth(int column) const;
  int contentWidth(int column) const;
  int contentWidthRecursive(int column, const QModelIndex &parent) const;

  int contentWidthForRow(int column, const QModelIndex &index,
                         bool includeChildren) const;

  int scrollbarAllowance() const;
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
  bool m_promoteToNotesEnabled = false;
};

#endif // EPISTEME_FILESYSTEMVIEW_H