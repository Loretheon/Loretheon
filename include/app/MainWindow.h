#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QString>

#include "DocumentManager.h"
#include "TextWidget.h"
#include "FileWidget.h"
#include "ChatWidget.h"
#include "EditSession.h"
#include "ThemeManager.h"
#include "inference/InferenceService.h"

class ModelDialog;
class OverseerDock;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();

private slots:
  void about();
  void aboutQt();
  void manageModels();
  void loadTheme(const QString &themeName);
  void onThemeSelected(const QString &theme);
  void toggleOverseer(bool visible);

private:
  void createActions();
  void createMenus();
  QSet<QString> modifiedPaths() const;
  void applyThemeToPalette(const ThemeTokens &tokens);
  void propagateTheme(const ThemeTokens &tokens);

  TextWidget *textWidget;
  FileWidget *fileWidget;
  DocumentManager *documentManager;
  InferenceService *inferenceService;
  EditSession *editSession;
  ChatWidget *chatWidget;
  ModelDialog *modelDialog = nullptr;
  ThemeManager *themeManager;
  OverseerDock *overseerDock = nullptr;

  QMenu *fileMenu;
  QMenu *newMenu;
  QMenu *toolsMenu;
  QMenu *themeMenu;
  QMenu *viewMenu;
  QMenu *helpMenu;

  QAction *newTextAct;
  QAction *newMarkdownAct;
  QAction *newPlantUmlAct;
  QAction *openAct;
  QAction *saveAct;
  QAction *exitAct;
  QAction *manageModelsAct;
  QAction *toggleOverseerAct;
  QAction *aboutAct;
  QAction *aboutQtAct;

  QString currentTheme;
};

#endif // MAINWINDOW_H