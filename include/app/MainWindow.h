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
class OverseerDock;
class LlmSettingsPanel;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();

private slots:
  void about();
  void aboutQt();
  void manageModels();
  void openLlmSettings();
  void loadTheme(const QString &themeName);
  void onThemeSelected(const QString &theme);
  void toggleOverseer(bool visible);

private:
  void createActions();
  void createMenus();
  QSet<QString> modifiedPaths() const;
  void applyThemeToPalette(const ThemeTokens &tokens);
  void propagateTheme(const ThemeTokens &tokens);

  void bindCurrentEditor(TextEdit *editor);

  DocumentArea *documentArea = nullptr;
  FileWidget *fileWidget = nullptr;
  DocumentManager *documentManager = nullptr;
  InferenceService *inferenceService = nullptr;
  EditSession *editSession = nullptr;
  ChatWidget *chatWidget = nullptr;
  ModelDialog *modelDialog = nullptr;
  LlmSettingsPanel *llmSettingsPanel = nullptr;
  ThemeManager *themeManager = nullptr;
  OverseerDock *overseerDock = nullptr;

  QMenu *fileMenu = nullptr;
  QMenu *newMenu = nullptr;
  QMenu *toolsMenu = nullptr;
  QMenu *themeMenu = nullptr;
  QMenu *viewMenu = nullptr;
  QMenu *helpMenu = nullptr;

  QAction *newTextAct = nullptr;
  QAction *newMarkdownAct = nullptr;
  QAction *newPlantUmlAct = nullptr;
  QAction *openAct = nullptr;
  QAction *saveAct = nullptr;
  QAction *exitAct = nullptr;
  QAction *manageModelsAct = nullptr;
  QAction *llmSettingsAct = nullptr;
  QAction *toggleOverseerAct = nullptr;
  QAction *aboutAct = nullptr;
  QAction *aboutQtAct = nullptr;

  QString currentTheme;
};

#endif // MAINWINDOW_H