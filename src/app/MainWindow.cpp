#include "MainWindow.h"
#include "LlmSettingsPanel.h"
#include "ChatWidget.h"
#include "OverseerDock.h"
#include "OverseerWidget.h"
#include "TextEdit.h"
#include "app/QfPaths.h"
#include "OverseerDock.h"
#include "OverseerWidget.h"
#include "inference/InferenceService.h"
#include "ui/ModelDialog.h"

#include "DocumentArea.h"
#include "EditSession.h"
#include "FileWidget.h"
#include "Settings.h"
#include "TextWidget.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

static InferenceService::LlmConfig configuredLlm() {
  InferenceService::LlmConfig config;

  const Settings::LlmSettings stored = Settings::getLlmSettings();

  if (stored.mode == QStringLiteral("remote")) {
    config.mode = InferenceService::LlmMode::Remote;
    config.endpoint = stored.endpoint;
    config.model = stored.model;
    config.apiKey = stored.apiKey;
    config.authType =
        stored.authType == QStringLiteral("none")
            ? InferenceService::LlmAuthType::None
            : InferenceService::LlmAuthType::Bearer;
  } else {
    config.mode = InferenceService::LlmMode::Local;
  }

  return config;
}

void MainWindow::openLlmSettings() {
  if (!llmSettingsPanel) {
    llmSettingsPanel = new LlmSettingsPanel(inferenceService, this);
  }

  llmSettingsPanel->show();
  llmSettingsPanel->raise();
  llmSettingsPanel->activateWindow();
}



MainWindow::MainWindow() {
  QWidget *widget = new QWidget;

  setCentralWidget(widget);

  setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  themeManager = new ThemeManager(this);

  fileWidget = new FileWidget(widget);
  documentManager = new DocumentManager(this);
  inferenceService = new InferenceService(this);

  const auto llmConfig = configuredLlm();

  inferenceService->initialize(
      LlamaManager::Backend::Vulkan, QFPaths::sttModelsDir(),
      InferenceService::SttModel::Nemotron35, llmConfig);

  documentArea = new DocumentArea(documentManager, widget);

  editSession = new EditSession(nullptr, this);
  chatWidget = new ChatWidget(inferenceService, editSession, this);

  documentArea->setEditSession(editSession);

  overseerDock = new OverseerDock(inferenceService, this);
  overseerDock->hide();

  addDockWidget(Qt::RightDockWidgetArea, overseerDock);

  connect(chatWidget, &ChatWidget::contextScopesChanged, documentArea,
          [this](const QStringList &scopeIds) {
            auto *editor = documentArea->currentEditor();
            if (!editor) {
              return;
            }
            if (scopeIds.isEmpty()) {
              editor->clearHighlightedScopes();
            } else {
              editor->setHighlightedScopes(scopeIds);
            }
          });

  connect(chatWidget, &ChatWidget::previewActivationRequested, documentArea,
          [this](bool active) {
            auto *textWidget = documentArea->currentTextWidget();
            if (textWidget) {
              textWidget->activatePreview(active);
            }
          });


  connect(documentArea, &DocumentArea::currentEditorChanged, this,
          &MainWindow::bindCurrentEditor, Qt::QueuedConnection);

  connect(documentArea, &DocumentArea::openDocumentRequested, documentManager,
          &DocumentManager::openFile);

  connect(documentArea, &DocumentArea::statusMessage, this,
          [this](const QString &text, int timeoutMs) {
            statusBar()->showMessage(text, timeoutMs);
          });

  connect(fileWidget, &FileWidget::fileSelected, documentManager,
          &DocumentManager::openFile);

  connect(fileWidget, &FileWidget::addToOverseerRequested, this,
        [this](const QStringList &paths) {
          if (paths.isEmpty()) {
            return;
          }

          if (!overseerDock) {
            return;
          }

          overseerDock->setVisible(true);

          if (auto *panel = overseerDock->overseerWidget()) {
            panel->addOverviewReferences(paths);
          }
        });

  connect(fileWidget, &FileWidget::newNoteRequested, documentManager,
          &DocumentManager::newMarkdownFileIn);

  connect(fileWidget, &FileWidget::newFolderRequested, documentManager,
          &DocumentManager::newFolderIn);

  connect(fileWidget, &FileWidget::deleteRequested, documentManager,
          &DocumentManager::deleteFile);

  connect(fileWidget, &FileWidget::convertToMarkdownRequested, documentManager,
          &DocumentManager::convertToMarkdown);

  connect(fileWidget, &FileWidget::convertToTextRequested, documentManager,
          &DocumentManager::convertToText);

  connect(fileWidget, &FileWidget::convertToPlantUmlRequested, documentManager,
          &DocumentManager::convertToPlantUml);

  connect(fileWidget, &FileWidget::convertToDotRequested, documentManager,
          &DocumentManager::convertToDot);

  connect(documentManager, &DocumentManager::documentCreated, fileWidget,
          &FileWidget::beginEditingPath);

  connect(documentManager, &DocumentManager::fileConverted, fileWidget,
          &FileWidget::beginEditingPath);

  connect(fileWidget, &FileWidget::renameRequested, documentManager,
          &DocumentManager::renameFile);

  connect(documentManager, &DocumentManager::currentDocumentChanged, fileWidget,
          [this](TextDocument *document) {
            fileWidget->setActivePath(document ? document->filePath()
                                               : QString());
            fileWidget->setModifiedPaths(modifiedPaths());
          });

  connect(documentManager, &DocumentManager::currentDocumentChanged, this,
          [this](TextDocument *document) {
            auto *textWidget = documentArea->currentTextWidget();
            if (!textWidget) {
              return;
            }
            const QString root =
                document && !document->filePath().isEmpty()
                    ? QFileInfo(document->filePath()).absolutePath()
                    : QString();
            textWidget->setProjectRoot(root);
          });

  connect(documentManager, &DocumentManager::currentDocumentChanged, this,
          [this](TextDocument *) {
            auto *editor = documentArea->currentEditor();
            if (editor && editSession) {
              editSession->setEditor(editor);
            }
            if (chatWidget && documentArea->currentEditor()) {
              chatWidget->setActiveEditor(documentArea->currentEditor());
            }
          });

  connect(themeManager, &ThemeManager::themeChanged, this,
          [this](const QString &name, const ThemeTokens &tokens) {
            currentTheme = name;
            applyThemeToPalette(tokens);
            propagateTheme(tokens);
          });

  QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, widget);

  mainSplitter->addWidget(fileWidget);

  QSplitter *rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);

  rightSplitter->addWidget(documentArea);
  rightSplitter->addWidget(chatWidget);

  mainSplitter->setSizes({240, 960});
  rightSplitter->setSizes({650, 300});

  QVBoxLayout *layout = new QVBoxLayout(widget);

  layout->setContentsMargins(5, 5, 5, 5);
  layout->addWidget(mainSplitter);

  setLayout(layout);

  createActions();
  createMenus();

  QSettings settings;
  currentTheme = settings.value("theme", ThemeRegistry::instance().defaultName())
                     .toString();
  loadTheme(currentTheme);

  setWindowTitle(tr("Episteme"));
  setMinimumSize(800, 800);

  QScreen *screen = QGuiApplication::primaryScreen();

  if (screen) setGeometry(screen->availableGeometry());
}

void MainWindow::bindCurrentEditor(TextEdit *editor) {
  if (!editor) {
    return;
  }

  if (editSession) {
    editSession->setEditor(editor);
  }

  if (chatWidget) {
    chatWidget->setActiveEditor(editor);
  }
}

void MainWindow::about() {
  QMessageBox::about(this, tr("About Episteme"),
                     tr("The <b>Episteme</b> document editor."));
}

void MainWindow::aboutQt() { QMessageBox::aboutQt(this, tr("About Qt")); }

void MainWindow::manageModels() {
  if (!modelDialog) {
    modelDialog = new ModelDialog(inferenceService, this);
  }

  modelDialog->show();
  modelDialog->raise();
  modelDialog->activateWindow();
}

void MainWindow::toggleOverseer(bool visible) {
  if (!overseerDock) {
    return;
  }

  overseerDock->setVisible(visible);
}

void MainWindow::loadTheme(const QString &themeName) {
  const QString resourcePath =
      QString(":/catppuccin-%1/stylesheet.qss").arg(themeName);

  QFile file(resourcePath);

  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Failed to load theme:" << themeName << "-"
             << file.errorString();
    return;
  }

  const QString stylesheet = QString::fromUtf8(file.readAll());
  file.close();

  qApp->setStyleSheet(stylesheet);

  themeManager->loadTheme(themeName, stylesheet);

  QSettings settings;
  settings.setValue("theme", themeName);
}

void MainWindow::onThemeSelected(const QString &theme) { loadTheme(theme); }

void MainWindow::applyThemeToPalette(const ThemeTokens &tokens) {
  QPalette pal = qApp->palette();

  pal.setColor(QPalette::Window, tokens.base);
  pal.setColor(QPalette::WindowText, tokens.text);
  pal.setColor(QPalette::Base, tokens.surface0);
  pal.setColor(QPalette::AlternateBase, tokens.mantle);
  pal.setColor(QPalette::Text, tokens.text);
  pal.setColor(QPalette::PlaceholderText, tokens.overlay0);
  pal.setColor(QPalette::Button, tokens.surface0);
  pal.setColor(QPalette::ButtonText, tokens.text);
  pal.setColor(QPalette::BrightText, tokens.red);
  pal.setColor(QPalette::Highlight, tokens.blue);
  pal.setColor(QPalette::HighlightedText, tokens.base);
  pal.setColor(QPalette::Link, tokens.blue);
  pal.setColor(QPalette::LinkVisited, tokens.mauve);
  pal.setColor(QPalette::ToolTipBase, tokens.surface0);
  pal.setColor(QPalette::ToolTipText, tokens.text);
  pal.setColor(QPalette::Light, tokens.surface2);
  pal.setColor(QPalette::Midlight, tokens.surface1);
  pal.setColor(QPalette::Dark, tokens.crust);
  pal.setColor(QPalette::Mid, tokens.overlay0);
  pal.setColor(QPalette::Shadow, tokens.crust);

  qApp->setPalette(pal);
}

void MainWindow::propagateTheme(const ThemeTokens &tokens) {
  if (documentArea) {
    documentArea->setThemeTokens(tokens);
  }
}

QSet<QString> MainWindow::modifiedPaths() const {
  QSet<QString> paths;

  if (!documentManager) return paths;

  for (TextDocument *document : documentManager->openDocuments()) {
    if (document->isModified() && !document->filePath().isEmpty()) {
      paths.insert(document->filePath());
    }
  }

  return paths;
}

void MainWindow::createActions() {
  auto getSafeIcon = [](const QString &themeIcon,
                        const QString &fallbackPath = "") -> QIcon {
    QIcon icon = QIcon::fromTheme(themeIcon);

    if (icon.isNull() && !fallbackPath.isEmpty()) {
      icon = QIcon(fallbackPath);
    }

    return icon;
  };

  newTextAct =
      new QAction(getSafeIcon("document-new", ":/icons/document-new.png"),
                  tr("&Text File"), this);
  newTextAct->setShortcuts(QKeySequence::New);
  newTextAct->setStatusTip(tr("Create a new plain text file"));

  connect(newTextAct, &QAction::triggered, documentManager,
          &DocumentManager::newTextFile);

  newMarkdownAct =
      new QAction(getSafeIcon("document-new", ":/icons/document-new.png"),
                  tr("&Markdown File"), this);
  newMarkdownAct->setStatusTip(tr("Create a new markdown file"));

  connect(newMarkdownAct, &QAction::triggered, documentManager,
          &DocumentManager::newMarkdownFile);

  newPlantUmlAct =
      new QAction(getSafeIcon("document-new", ":/icons/document-new.png"),
                  tr("&PlantUML Diagram"), this);
  newPlantUmlAct->setStatusTip(tr("Create a new PlantUML diagram"));

  connect(newPlantUmlAct, &QAction::triggered, documentManager,
          &DocumentManager::newPlantUmlFile);

  openAct =
      new QAction(getSafeIcon("document-open", ":/icons/document-open.png"),
                  tr("&Open..."), this);
  openAct->setShortcuts(QKeySequence::Open);
  openAct->setStatusTip(tr("Open an existing file"));

  connect(openAct, &QAction::triggered, this, [this]() {
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
                                     tr("Text Files (*.txt);;"
                                        "Markdown Files (*.md);;"
                                        "PlantUML Files (*.puml *.plantuml);;"
                                        "Graphviz Files (*.dot *.gv);;"
                                        "Mermaid Files (*.mmd *.mermaid);;"
                                        "All Files (*)"));

    if (!path.isEmpty()) {
      documentManager->openFile(path);
    }
  });

  saveAct =
      new QAction(getSafeIcon("document-save", ":/icons/document-save.png"),
                  tr("&Save"), this);
  saveAct->setShortcuts(QKeySequence::Save);
  saveAct->setStatusTip(tr("Save the document to disk"));

  connect(saveAct, &QAction::triggered, documentManager,
          &DocumentManager::save);

  connect(documentManager, &DocumentManager::currentDocumentChanged, this,
          [this](TextDocument *) {
            fileWidget->setModifiedPaths(modifiedPaths());
          });

  connect(documentManager, &DocumentManager::documentCreated, this,
          [this](const QString &) {
            fileWidget->setModifiedPaths(modifiedPaths());
          });

  connect(documentManager, &DocumentManager::fileRenamed, this,
          [this](const QString &, const QString &) {
            fileWidget->setModifiedPaths(modifiedPaths());
          });

  connect(documentManager, &DocumentManager::fileConverted, this,
          [this](const QString &, const QString &) {
            fileWidget->setModifiedPaths(modifiedPaths());
          });

  exitAct = new QAction(
      getSafeIcon("application-exit", ":/icons/application-exit.png"),
      tr("E&xit"), this);
  exitAct->setShortcuts(QKeySequence::Quit);
  exitAct->setStatusTip(tr("Exit the application"));

  connect(exitAct, &QAction::triggered, this, &QWidget::close);

  manageModelsAct = new QAction(tr("&Manage Models..."), this);
  manageModelsAct->setStatusTip(tr("Download or select LLM and speech models"));

  connect(manageModelsAct, &QAction::triggered, this,
          &MainWindow::manageModels);

  llmSettingsAct = new QAction(tr("LLM &Settings..."), this);
  llmSettingsAct->setStatusTip(tr("Configure the LLM endpoint and credentials"));

  connect(llmSettingsAct, &QAction::triggered, this,
          &MainWindow::openLlmSettings);

  toggleOverseerAct = new QAction(tr("Show &Overseer"), this);
  toggleOverseerAct->setCheckable(true);
  toggleOverseerAct->setChecked(false);
  toggleOverseerAct->setStatusTip(
      tr("Toggle the Overseer persistent assistant panel"));

  connect(toggleOverseerAct, &QAction::toggled, this,
          &MainWindow::toggleOverseer);

  connect(overseerDock, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (toggleOverseerAct) {
              QSignalBlocker blocker(toggleOverseerAct);
              toggleOverseerAct->setChecked(visible);
            }
          });

  aboutAct = new QAction(getSafeIcon("help-about", ":/icons/help-about.png"),
                         tr("&About"), this);
  aboutAct->setStatusTip(tr("Show the application's About box"));

  connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

  aboutQtAct = new QAction(tr("About &Qt"), this);
  aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));

  connect(aboutQtAct, &QAction::triggered, this, &MainWindow::aboutQt);
}

void MainWindow::createMenus() {
  fileMenu = menuBar()->addMenu(tr("&File"));

  newMenu = fileMenu->addMenu(tr("&New"));

  newMenu->addAction(newTextAct);
  newMenu->addAction(newMarkdownAct);
  newMenu->addAction(newPlantUmlAct);

  fileMenu->addAction(openAct);
  fileMenu->addAction(saveAct);

  fileMenu->addSeparator();

  fileMenu->addAction(exitAct);

  viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(toggleOverseerAct);

  toolsMenu = menuBar()->addMenu(tr("&Tools"));

  toolsMenu->addAction(llmSettingsAct);
  toolsMenu->addSeparator();
  toolsMenu->addAction(manageModelsAct);

  themeMenu = menuBar()->addMenu(tr("&Theme"));

  QActionGroup *themeGroup = new QActionGroup(this);
  themeGroup->setExclusive(true);

  const QStringList themes = ThemeRegistry::instance().names();

  for (const QString &theme : themes) {
    QAction *themeAction = themeMenu->addAction(theme);
    themeAction->setCheckable(true);
    themeGroup->addAction(themeAction);

    if (theme == currentTheme) themeAction->setChecked(true);

    connect(themeAction, &QAction::triggered, this,
            [this, theme]() { onThemeSelected(theme); });
  }

  helpMenu = menuBar()->addMenu(tr("&Help"));

  helpMenu->addAction(aboutAct);
  helpMenu->addAction(aboutQtAct);
}