#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QWidget>

#include "WorkstationBodyFactory.h"
#include "WorkstationWindow.h"

class EditSession;
class TextDocument;
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

  void applyTiling();

  void setAllTiled();
  void setAllFloating();
  void arrangeAll();

  void setFileStatus(const QString &absolutePath, const QString &status);

  QList<WorkstationWindow *> windows() const { return m_windows; }
  WorkstationWindow *focusedWindow() const { return m_focusedWindow; }
  WorkstationWindow *windowForPath(const QString &absolutePath) const;

  void focusWindow(WorkstationWindow *window);

  void reloadWindowFromDisk(WorkstationWindow *window);
  void saveWindowToDisk(WorkstationWindow *window);

signals:
  void windowClosed(const QString &absolutePath);
  void currentFileChanged(const QString &absolutePath);
  void windowOpened(const QString &absolutePath);
  void windowListChanged();

  void rewriteRequested(WorkstationWindow *window);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
  void onWindowCloseRequested(WorkstationWindow *window);
  void onWindowFocusRequested(WorkstationWindow *window);
  void onWindowGeometryChanged(WorkstationWindow *window);
  void onWindowDragStarted(WorkstationWindow *window);
  void onWindowDragMoved(WorkstationWindow *window, const QPoint &globalPos);
  void onWindowDragFinished(WorkstationWindow *window,
                            const QPoint &globalPos);
  void onWindowResizeFinished(WorkstationWindow *window);
  void onWindowModeChangeRequested(WorkstationWindow *window,
                                   WorkstationWindow::Mode mode);

  void onDiskFileChanged(const QString &path);

  void onDiskConflictReload(WorkstationWindow *window);
  void onDiskConflictKeepMine(WorkstationWindow *window);
  void onDiskConflictOverwrite(WorkstationWindow *window);

private:
  void destroyWindow(WorkstationWindow *window, bool emitSignals);

  int nextZ();

  void bringToFront(WorkstationWindow *window);
  void updateAlsoOpenBadges();
  void clearDragHighlights();

  QString absolutePathFor(const QString &relativePath) const;
  QString layoutPath() const;
  QString sessionFolder() const;

  // Tiling.
  QList<WorkstationWindow *> tiledWindows() const;
  QList<WorkstationWindow *> floatingWindows() const;
  QList<QRect> computeTiledRects(int count) const;

  // Find the nearest tiled window to a global point, used for the
  // swap-on-drop gesture.
  WorkstationWindow *nearestTiledWindow(const QPoint &globalPos,
                                        WorkstationWindow *ignore) const;

  void watchFile(const QString &absolutePath);
  void unwatchFile(const QString &absolutePath);

  DocumentManager *m_manager = nullptr;
  EditSession *m_editSession = nullptr;

  WorkstationBodyFactory m_bodyFactory;

  QString m_outputFolder;

  QList<WorkstationWindow *> m_windows;
  QHash<QString, WorkstationWindow *> m_byPath;

  WorkstationWindow *m_focusedWindow = nullptr;
  WorkstationWindow *m_dropTarget = nullptr;

  QFileSystemWatcher m_fileWatcher;
  QSet<QString> m_ignoreNextChange;

  int m_zCounter = 0;

  bool m_loading = false;

  static constexpr int kGap = 12;
  static constexpr int kMargin = 12;
};