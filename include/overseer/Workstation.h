#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

#include "WorkstationBodyFactory.h"

class EditSession;
class TextDocument;
class WorkstationWindow;
class DocumentManager;

class Workstation : public QWidget {
  Q_OBJECT

public:
  Workstation(DocumentManager *manager, EditSession *editSession,
              QWidget *parent = nullptr);

  ~Workstation() override;

  void setOutputFolder(const QString &folder);

  void loadLayout();
  void saveLayout();
  void resetLayout();

  WorkstationWindow *openFile(const QString &absolutePath,
                              const QString &bodyHint = QString());

  void closeFile(const QString &absolutePath);

  void closeAll();

  void refreshAlsoOpenBadges();

  void tile();
  void cascade();
  void autoArrange();

  void setFileStatus(const QString &absolutePath, const QString &status);

  QList<WorkstationWindow *> windows() const { return m_windows; }

  WorkstationWindow *focusedWindow() const { return m_focusedWindow; }

  void focusWindow(WorkstationWindow *window);

  // Reload the given window's document from disk, discarding in-memory
  // edits. Called by the conflict banner's "Reload" button and by the
  // context menu.
  void reloadWindowFromDisk(WorkstationWindow *window);

  // Save the given window's document to disk, overwriting whatever is
  // there. Called by the conflict banner's "Overwrite" button and by
  // the context menu.
  void saveWindowToDisk(WorkstationWindow *window);

signals:
  void windowClosed(const QString &absolutePath);
  void currentFileChanged(const QString &absolutePath);
  void windowOpened(const QString &absolutePath);
  void windowListChanged();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
  void onWindowCloseRequested(WorkstationWindow *window);
  void onWindowFocusRequested(WorkstationWindow *window);
  void onWindowGeometryChanged(WorkstationWindow *window);
  void onWindowDragFinished(WorkstationWindow *window);
  void onWindowResizeFinished(WorkstationWindow *window);

  void onDiskFileChanged(const QString &path);

  void onDiskConflictReload(WorkstationWindow *window);
  void onDiskConflictKeepMine(WorkstationWindow *window);
  void onDiskConflictOverwrite(WorkstationWindow *window);

private:
  WorkstationWindow *windowForPath(const QString &absolutePath) const;

  int nextZ();

  void bringToFront(WorkstationWindow *window);
  void updateAlsoOpenBadges();

  QString absolutePathFor(const QString &relativePath) const;
  QString layoutPath() const;
  QString sessionFolder() const;

  QSize defaultNewWindowSize() const;
  QRect findFreeSlot(const QSize &size,
                     WorkstationWindow *ignore = nullptr) const;
  bool rectOverlapsAny(const QRect &candidate,
                       WorkstationWindow *ignore = nullptr) const;

  QRect resolveNonOverlappingRect(const QRect &target,
                                  WorkstationWindow *window) const;
  QRect resolveNonOverlappingResize(const QRect &target,
                                    WorkstationWindow *window) const;

  void watchFile(const QString &absolutePath);
  void unwatchFile(const QString &absolutePath);

  DocumentManager *m_manager = nullptr;
  EditSession *m_editSession = nullptr;

  WorkstationBodyFactory m_bodyFactory;

  QString m_outputFolder;

  QList<WorkstationWindow *> m_windows;
  QHash<QString, WorkstationWindow *> m_byPath;

  WorkstationWindow *m_focusedWindow = nullptr;

  // Watch every open file for changes on disk. When the file changes
  // and the in-memory document has no unsaved edits, reload it. When
  // it has unsaved edits, show a conflict banner in the window.
  QFileSystemWatcher m_fileWatcher;

  // Files whose on-disk change we have already suppressed because we
  // wrote them ourselves. Cleared on the next event-loop turn.
  QSet<QString> m_ignoreNextChange;

  int m_zCounter = 0;

  static constexpr int kGap = 12;
  static constexpr int kMargin = 12;
  static constexpr int kMinGap = 6;
};