#pragma once

#include "ThemeTokens.h"

#include <QToolButton>
#include <QWidget>

class OverseerSessionManager;
class FileWidget;
class AutoHideDock;
class ChatWidget;
class DocumentArea;
class DocumentManager;
class EditSession;
class InferenceService;
class OverseerWidget;
class Workstation;
class TextDocument;
class TextEdit;
class ConductorDock;
class QSplitter;
class QStackedWidget;

class OverseerPage : public QWidget {
  Q_OBJECT

public:
  OverseerPage(InferenceService *inferenceService,
             EditSession *editSession,
             OverseerSessionManager *manager,
             QWidget *parent = nullptr);

  ~OverseerPage() override;

  OverseerWidget *overseerWidget() const { return m_overseer; }
  Workstation *workstation() const { return m_workstation; }
  DocumentManager *documentManager() const { return m_documentManager; }
  DocumentArea *documentArea() const { return m_documentArea; }

  void setThemeTokens(const ThemeTokens &tokens);

  bool hasUnsavedChanges() const;
  bool saveAll();
  void discardAll();
  FileWidget *fileWidget() const { return m_fileWidget; }
public slots:
  void stageFileInSession(const QString &absolutePath);

signals:
  void statusMessage(const QString &text, int timeoutMs);
  void openFileInNormalEditorRequested(const QString &absolutePath);
  void dirtyChanged(bool dirty);

private slots:
  void onSessionSelected(const QString &name);
  void onSessionCleared();
  void onFileSelected(const QString &path);
  void onDocumentChanged();
  void onAddToOverview(const QStringList &paths);
  void onFocusedFileChanged(const QString &absolutePath);
  void setWorkstationOnManager();

private:
  void reloadSession(const QString &name);
  void closeAllSessionDocuments();
  void migrateLegacyLayoutFiles();

  // Show or hide the session-scoped file tree, and collapse or restore
  // its pane in the left splitter accordingly.  The tree has no
  // meaningful root before a session is chosen, so it stays hidden.
  void updateFileTreeVisibility();

  ConductorDock *m_conductorDock = nullptr;
  QToolButton *m_dockTrigger = nullptr;
  InferenceService *m_inferenceService = nullptr;
  EditSession *m_editSession = nullptr;
  OverseerSessionManager *m_manager = nullptr;
  OverseerWidget *m_overseer = nullptr;
  Workstation *m_workstation = nullptr;

  DocumentManager *m_documentManager = nullptr;
  DocumentArea *m_documentArea = nullptr;
  FileWidget *m_fileWidget = nullptr;

  AutoHideDock *m_leftDock = nullptr;
  AutoHideDock *m_rightDock = nullptr;

  QSplitter *m_mainSplitter = nullptr;

  // The vertical splitter that holds the session list above the file
  // tree in the left dock.  We keep a pointer so updateFileTreeVisibility
  // can collapse the tree's pane when the tree is hidden.
  QSplitter *m_leftSplitter = nullptr;

  QString m_currentSessionName;
  QString m_currentOutputFolder;
};