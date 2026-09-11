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

  connect(documentManager, &DocumentManager::documentCreated, fileWidget,
          &FileWidget::beginEditingPath);

  connect(fileWidget, &FileWidget::renameRequested, documentManager,
          &DocumentManager::renameFile);

  connect(documentManager, &DocumentManager::documentChanged, textWidget,
          &TextWidget::setActiveDocument);

  connect(documentManager, &DocumentManager::documentChanged, chatWidget,
          [this](TextDocument *) {
            chatWidget->setActiveEditor(textWidget->editor());
          });

  /*
   * Keep the edit session synchronized with the active editor too.
   *
   * In the current application there is one TextEdit owned by
   * TextWidget, but keeping this explicit makes the dependency clear.
   */
  connect(
      documentManager, &DocumentManager::documentChanged, this,
      [this](TextDocument *) { editSession->setEditor(textWidget->editor()); });

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

void MainWindow::createActions() {
  newTextAct = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentNew),
                           tr("&Text File"), this);

  newTextAct->setShortcuts(QKeySequence::New);

  newTextAct->setStatusTip(tr("Create a new plain text file"));

  connect(newTextAct, &QAction::triggered, documentManager,
          &DocumentManager::newTextFile);

  newMarkdownAct = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentNew),
                               tr("&Markdown File"), this);

  newMarkdownAct->setStatusTip(tr("Create a new markdown file"));

  connect(newMarkdownAct, &QAction::triggered, documentManager,
          &DocumentManager::newMarkdownFile);

  openAct = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen),
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

  saveAct = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSave),
                        tr("&Save"), this);

  saveAct->setShortcuts(QKeySequence::Save);

  saveAct->setStatusTip(tr("Save the document to disk"));

  connect(saveAct, &QAction::triggered, documentManager,
          &DocumentManager::save);

  exitAct = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::ApplicationExit),
                        tr("E&xit"), this);

  exitAct->setShortcuts(QKeySequence::Quit);

  exitAct->setStatusTip(tr("Exit the application"));

  connect(exitAct, &QAction::triggered, this, &QWidget::close);

  manageModelsAct = new QAction(tr("&Manage Models..."), this);

  manageModelsAct->setStatusTip(tr("Download or select LLM and speech models"));

  connect(manageModelsAct, &QAction::triggered, this,
          &MainWindow::manageModels);

  aboutAct = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::HelpAbout),
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

  fileMenu->addAction(openAct);

  fileMenu->addAction(saveAct);

  fileMenu->addSeparator();

  fileMenu->addAction(exitAct);

  toolsMenu = menuBar()->addMenu(tr("&Tools"));

  toolsMenu->addAction(manageModelsAct);

  helpMenu = menuBar()->addMenu(tr("&Help"));

  helpMenu->addAction(aboutAct);

  helpMenu->addAction(aboutQtAct);
}