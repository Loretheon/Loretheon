#ifndef EPISTEME_MAINWINDOW_H
#define EPISTEME_MAINWINDOW_H

#include <QMainWindow>

#include "../ai/chat/ChatWidget.h"
#include "../ai/edit/EditSession.h"
#include "DocumentManager.h"
#include "FileWidget.h"
#include "TextWidget.h"
#include "inference/InferenceService.h"

class QMenu;
class QAction;
class ModelDialog;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();

private:
  void createActions();
  void createMenus();
  void loadTheme(const QString &themeName);

  QMenu *fileMenu = nullptr;
  QMenu *newMenu = nullptr;
  QMenu *toolsMenu = nullptr;
  QMenu *themeMenu = nullptr;
  QMenu *helpMenu = nullptr;

  QAction *newTextAct = nullptr;
  QAction *newMarkdownAct = nullptr;
  QAction *openAct = nullptr;
  QAction *saveAct = nullptr;
  QAction *exitAct = nullptr;
  QAction *manageModelsAct = nullptr;
  QAction *aboutAct = nullptr;
  QAction *aboutQtAct = nullptr;

  DocumentManager *documentManager = nullptr;
  TextWidget *textWidget = nullptr;
  FileWidget *fileWidget = nullptr;

  InferenceService *inferenceService = nullptr;
  EditSession *editSession = nullptr;
  ChatWidget *chatWidget = nullptr;
  ModelDialog *modelDialog = nullptr;

  QString currentTheme;

private slots:
  void about();
  void aboutQt();
  void manageModels();
  void onThemeSelected(const QString &theme);
};

#endif // EPISTEME_MAINWINDOW_H