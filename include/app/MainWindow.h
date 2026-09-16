#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QString>

#include "DocumentManager.h"
#include "ThemeManager.h"
#include "inference/InferenceService.h"

class DocumentArea;
class TextEdit;
class TextDocument;
class FileWidget;
class ChatWidget;
class EditSession;
class ModelDialog;
class LlmSettingsPanel;
class OverseerPage;
class Workstation;

class QAction;
class QMenu;
class QStackedWidget;
class QToolBar;
class QToolButton;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void about();
  void aboutQt();
  void manageModels();
  void openLlmSettings();
  void onThemeSelected(const QString &theme);
  void onOverseerThemeSelected(const QString &theme);
  void onModeToggled(bool overseerMode);

private:
  void createActions();
  void createMenus();
  void createToolbar();

  void buildNormalPage();
  void buildOverseerPage();

  void applyNormalTheme(const QString &name);
  void applyOverseerTheme(const QString &name);

  QPalette paletteForTokens(const ThemeTokens &tokens) const;

  QSet<QString> modifiedPaths() const;
  void bindCurrentEditor(TextEdit *editor);

  bool confirmDiscardChanges(const QString &areaName);

  // --- Normal page -------------------------------------------------------

  QWidget *m_normalPage = nullptr;
  DocumentArea *m_documentArea = nullptr;
  FileWidget *m_fileWidget = nullptr;
  DocumentManager *m_documentManager = nullptr;
  EditSession *m_editSession = nullptr;
  ChatWidget *m_chatWidget = nullptr;

  // --- Overseer page -----------------------------------------------------

  OverseerPage *m_overseerPage = nullptr;

  // --- Shared ------------------------------------------------------------

  QStackedWidget *m_centralStack = nullptr;
  QToolBar *m_topToolBar = nullptr;
  QToolButton *m_modeButton = nullptr;

  InferenceService *m_inferenceService = nullptr;
  ModelDialog *m_modelDialog = nullptr;
  LlmSettingsPanel *m_llmSettingsPanel = nullptr;

  ThemeManager *m_normalThemeManager = nullptr;
  ThemeManager *m_overseerThemeManager = nullptr;

  QString m_currentNormalTheme;
  QString m_currentOverseerTheme;

  QMenu *m_fileMenu = nullptr;
  QMenu *m_newMenu = nullptr;
  QMenu *m_toolsMenu = nullptr;
  QMenu *m_themeMenu = nullptr;
  QMenu *m_viewMenu = nullptr;
  QMenu *m_helpMenu = nullptr;

  QAction *m_newTextAct = nullptr;
  QAction *m_newMarkdownAct = nullptr;
  QAction *m_newPlantUmlAct = nullptr;
  QAction *m_openAct = nullptr;
  QAction *m_saveAct = nullptr;
  QAction *m_saveAllAct = nullptr;
  QAction *m_exitAct = nullptr;
  QAction *m_manageModelsAct = nullptr;
  QAction *m_llmSettingsAct = nullptr;
  QAction *m_toggleModeAct = nullptr;
  QAction *m_aboutAct = nullptr;
  QAction *m_aboutQtAct = nullptr;
};

#endif // MAINWINDOW_H