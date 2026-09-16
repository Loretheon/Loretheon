#pragma once

#include "ThemeTokens.h"

#include <QWidget>

class ChatWidget;
class DocumentArea;
class DocumentManager;
class EditSession;
class FileWidget;
class InferenceService;
class OverseerWidget;
class Workstation;

class QSplitter;
class QStackedWidget;

class OverseerPage : public QWidget {
  Q_OBJECT

public:
  OverseerPage(InferenceService *inferenceService,
               EditSession *editSession, QWidget *parent = nullptr);

  ~OverseerPage() override;

  OverseerWidget *overseerWidget() const { return m_overseer; }
  Workstation *workstation() const { return m_workstation; }
  DocumentManager *documentManager() const { return m_documentManager; }
  DocumentArea *documentArea() const { return m_documentArea; }

  void setThemeTokens(const ThemeTokens &tokens);

  bool hasUnsavedChanges() const;
  bool saveAll();
  void discardAll();

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

private:
  void reloadSession(const QString &name);
  void closeAllSessionDocuments();

  InferenceService *m_inferenceService = nullptr;
  EditSession *m_editSession = nullptr;

  OverseerWidget *m_overseer = nullptr;
  Workstation *m_workstation = nullptr;

  DocumentManager *m_documentManager = nullptr;
  DocumentArea *m_documentArea = nullptr;
  FileWidget *m_fileWidget = nullptr;

  QSplitter *m_mainSplitter = nullptr;

  QString m_currentSessionName;
  QString m_currentOutputFolder;
};