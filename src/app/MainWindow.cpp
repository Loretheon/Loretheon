#include "MainWindow.h"

#include "app/QfPaths.h"
#include "ui/ModelDialog.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QSettings>
#include <QActionGroup>

static InferenceService::LlmConfig configuredLlm() {
  InferenceService::LlmConfig config;

  const QString mode =
      qEnvironmentVariable("TALOS_LLM_MODE").trimmed().toLower();

  if (mode == QStringLiteral("remote")) {
    config.mode = InferenceService::LlmMode::Remote;

    config.endpoint = qEnvironmentVariable("TALOS_LLM_URL").trimmed();

    config.model = qEnvironmentVariable("TALOS_LLM_MODEL").trimmed();

    config.apiKey = qEnvironmentVariable("TALOS_LLM_API_KEY").trimmed();

    const QString auth =
        qEnvironmentVariable("TALOS_LLM_AUTH").trimmed().toLower();

    config.authType = auth == QStringLiteral("none")
                          ? InferenceService::LlmAuthType::None
                          : InferenceService::LlmAuthType::Bearer;

  } else {
    config.mode = InferenceService::LlmMode::Local;
  }

  return config;
}

MainWindow::MainWindow() {
  QWidget *widget = new QWidget;

  setCentralWidget(widget);

  setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);

  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);

  setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  textWidget = new TextWidget(this);

  fileWidget = new FileWidget(widget);

  documentManager = new DocumentManager(this);

  inferenceService = new InferenceService(this);
  //
  // inferenceService->initialize(LlamaManager::Backend::Vulkan,
  //                              QFPaths::sttModelsDir());

  const auto llmConfig = configuredLlm();

  inferenceService->initialize(
      LlamaManager::Backend::Vulkan, QFPaths::sttModelsDir(),
      InferenceService::SttModel::Nemotron35, llmConfig);

  // inferenceService->setModelDirectory(QFPaths::llmModelsDir());

  editSession = new EditSession(textWidget->editor(), this);

  chatWidget = new ChatWidget(inferenceService, editSession, this);

  connect(fileWidget, &FileWidget::fileSelected, documentManager,
          &DocumentManager::openFile);

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

  connect(documentManager, &DocumentManager::documentCreated, fileWidget,
          &FileWidget::beginEditingPath);

  connect(documentManager, &DocumentManager::fileConverted, fileWidget,
          &FileWidget::beginEditingPath);

  connect(fileWidget, &FileWidget::renameRequested, documentManager,
          &DocumentManager::renameFile);

  connect(documentManager, &DocumentManager::documentChanged, textWidget,
          &TextWidget::setActiveDocument);

  connect(documentManager, &DocumentManager::documentChanged, fileWidget,
          [this](TextDocument *document) {
            fileWidget->setActivePath(document ? document->filePath()
                                               : QString());
            fileWidget->setModifiedPaths(modifiedPaths());
          });

  connect(documentManager, &DocumentManager::documentChanged, chatWidget,
          [this](TextDocument *) {
            chatWidget->setActiveEditor(textWidget->editor());
          });

  connect(
      documentManager, &DocumentManager::documentChanged, this,
      [this](TextDocument *) { editSession->setEditor(textWidget->editor()); });

  connect(textWidget->editor()->document(), &QTextDocument::modificationChanged,
          this, [this](bool) {
            fileWidget->setModifiedPaths(modifiedPaths());
          });

  /*
   * Left:
   *     File manager
   *
   * Right:
   *     Text editor
   *     AI chat
   */
  QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, widget);

  mainSplitter->addWidget(fileWidget);

  /*
   * The right side gets its own vertical splitter so the editor
   * and chat remain independently resizable.
   */
  QSplitter *rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);

  rightSplitter->addWidget(textWidget);

  rightSplitter->addWidget(chatWidget);

  /*
   * Give the file manager a useful but relatively narrow width.
   * The editor gets most of the space.
   */
  mainSplitter->setSizes({240, 960});

  /*
   * Give the editor more vertical space than the chat.
   */
  rightSplitter->setSizes({650, 300});

  QVBoxLayout *layout = new QVBoxLayout(widget);

  layout->setContentsMargins(5, 5, 5, 5);

  layout->addWidget(mainSplitter);

  setLayout(layout);

  createActions();
  createMenus();

  // Load saved theme or default to mocha
  QSettings settings;
  currentTheme = settings.value("theme", "mocha").toString();
  loadTheme(currentTheme);

  setWindowTitle(tr("Episteme"));

  setMinimumSize(800, 800);

  QScreen *screen = QGuiApplication::primaryScreen();

  if (screen) {
    setGeometry(screen->availableGeometry());
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

void MainWindow::loadTheme(const QString &themeName) {
  QString resourcePath = QString(":/catppuccin-%1/stylesheet.qss").arg(themeName);

  QFile file(resourcePath);

  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Failed to load theme:" << themeName << "-" << file.errorString();
    return;
  }

  const QString stylesheet = QString::fromUtf8(file.readAll());
  file.close();

  qDebug() << "Loaded theme:" << themeName << ", stylesheet size:" << stylesheet.size();
  qApp->setStyleSheet(stylesheet);

  currentTheme = themeName;

  // Save theme preference
  QSettings settings;
  settings.setValue("theme", themeName);
}

void MainWindow::onThemeSelected(const QString &theme) {
  loadTheme(theme);

}

QSet<QString> MainWindow::modifiedPaths() const {
  QSet<QString> paths;

  if (!documentManager)
    return paths;

  TextDocument *current = documentManager->currentDocument();
  if (current && current->isModified() && !current->filePath().isEmpty())
    paths.insert(current->filePath());

  return paths;
}

void MainWindow::createActions() {
  // Helper function for safe icon loading
  auto getSafeIcon = [](const QString &themeIcon,
                        const QString &fallbackPath = "") -> QIcon {
    QIcon icon = QIcon::fromTheme(themeIcon);

    if (icon.isNull() && !fallbackPath.isEmpty()) {
      icon = QIcon(fallbackPath);
    }

    return icon;
  };

  // Do NOT force an icon theme.
  // Qt will use the user's desktop theme (Adwaita, Breeze, etc.).

  // New Text File Action
  newTextAct =
      new QAction(getSafeIcon("document-new", ":/icons/document-new.png"),
                  tr("&Text File"), this);
  newTextAct->setShortcuts(QKeySequence::New);
  newTextAct->setStatusTip(tr("Create a new plain text file"));

  connect(newTextAct, &QAction::triggered, documentManager,
          &DocumentManager::newTextFile);

  // New Markdown File Action
  newMarkdownAct =
      new QAction(getSafeIcon("document-new", ":/icons/document-new.png"),
                  tr("&Markdown File"), this);
  newMarkdownAct->setStatusTip(tr("Create a new markdown file"));

  connect(newMarkdownAct, &QAction::triggered, documentManager,
          &DocumentManager::newMarkdownFile);

  // Open Action
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
                                        "All Files (*)"));

    if (!path.isEmpty()) {
      documentManager->openFile(path);
    }
  });

  // Save Action
  saveAct =
      new QAction(getSafeIcon("document-save", ":/icons/document-save.png"),
                  tr("&Save"), this);
  saveAct->setShortcuts(QKeySequence::Save);
  saveAct->setStatusTip(tr("Save the document to disk"));

  connect(saveAct, &QAction::triggered, documentManager,
          &DocumentManager::save);

  connect(documentManager, &DocumentManager::documentChanged, this,
          [this](TextDocument *) { fileWidget->setModifiedPaths(modifiedPaths()); });

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

  // Exit Action
  exitAct = new QAction(
      getSafeIcon("application-exit", ":/icons/application-exit.png"),
      tr("E&xit"), this);
  exitAct->setShortcuts(QKeySequence::Quit);
  exitAct->setStatusTip(tr("Exit the application"));

  connect(exitAct, &QAction::triggered, this, &QWidget::close);

  // Manage Models Action
  manageModelsAct = new QAction(tr("&Manage Models..."), this);
  manageModelsAct->setStatusTip(tr("Download or select LLM and speech models"));

  connect(manageModelsAct, &QAction::triggered, this,
          &MainWindow::manageModels);

  // About Action
  aboutAct = new QAction(getSafeIcon("help-about", ":/icons/help-about.png"),
                         tr("&About"), this);
  aboutAct->setStatusTip(tr("Show the application's About box"));

  connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

  // About Qt Action
  aboutQtAct = new QAction(tr("About &Qt"), this);
  aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));

  connect(aboutQtAct, &QAction::triggered, this, &MainWindow::aboutQt);
}

void MainWindow::createMenus() {
  fileMenu = menuBar()->addMenu(tr("&File"));

  newMenu = fileMenu->addMenu(tr("&New"));

  newMenu->addAction(newTextAct);

  newMenu->addAction(newMarkdownAct);

  fileMenu->addAction(openAct);

  fileMenu->addAction(saveAct);

  fileMenu->addSeparator();

  fileMenu->addAction(exitAct);

  toolsMenu = menuBar()->addMenu(tr("&Tools"));

  toolsMenu->addAction(manageModelsAct);

  // Theme menu
  themeMenu = menuBar()->addMenu(tr("&Theme"));

  QActionGroup *themeGroup = new QActionGroup(this);
  themeGroup->setExclusive(true);

  const QStringList themes = {"frappe", "latte", "macchiato", "mocha"};

  for (const QString &theme : themes) {
    QAction *themeAction = themeMenu->addAction(theme);
    themeAction->setCheckable(true);
    themeGroup->addAction(themeAction);

    if (theme == currentTheme) {
      themeAction->setChecked(true);
    }

    connect(themeAction, &QAction::triggered, this, [this, theme]() {
      onThemeSelected(theme);
    });
  }

  helpMenu = menuBar()->addMenu(tr("&Help"));

  helpMenu->addAction(aboutAct);

  helpMenu->addAction(aboutQtAct);
}