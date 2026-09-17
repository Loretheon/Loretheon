#include "MainWindow.h"

#include "ChatWidget.h"
#include "DocumentArea.h"
#include "EditSession.h"
#include "FileWidget.h"
#include "LlmSettingsPanel.h"
#include "OverseerPage.h"
#include "Settings.h"
#include "TextEdit.h"
#include "TextWidget.h"
#include "app/QfPaths.h"
#include "ThemeTokens.h"
#include "inference/InferenceService.h"
#include "ui/ModelDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
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
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr auto NormalThemeKey = "theme";
constexpr auto OverseerThemeKey = "overseer/theme";
constexpr auto ModeKey = "overseer/mode";

InferenceService::LlmConfig configuredLlm() {
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

} // namespace

MainWindow::MainWindow() {
  setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  m_normalThemeManager = new ThemeManager(this);
  m_overseerThemeManager = new ThemeManager(this);

  m_inferenceService = new InferenceService(this);

  m_inferenceService->initialize(
      LlamaManager::Backend::Vulkan, QFPaths::sttModelsDir(),
      InferenceService::SttModel::Nemotron35, configuredLlm());

  m_editSession = new EditSession(nullptr, this);

  buildNormalPage();
  buildOverseerPage();

  m_centralStack = new QStackedWidget(this);
  m_centralStack->addWidget(m_normalPage);
  m_centralStack->addWidget(m_overseerPage);

  setCentralWidget(m_centralStack);

  createActions();
  createToolbar();
  createMenus();

  QSettings settings;

  m_currentNormalTheme =
      settings.value(NormalThemeKey, ThemeRegistry::instance().defaultName())
          .toString();

  m_currentOverseerTheme =
      settings.value(OverseerThemeKey, QString()).toString();

  if (m_currentOverseerTheme.isEmpty()) {
    m_currentOverseerTheme = m_currentNormalTheme;
  }

  const bool overseerMode = settings.value(ModeKey, false).toBool();

  m_centralStack->setCurrentIndex(overseerMode ? 1 : 0);

  applyNormalTheme(m_currentNormalTheme);
  applyOverseerTheme(m_currentOverseerTheme);

  ThemeRegistry::instance().setActiveTheme(
      overseerMode ? m_currentOverseerTheme : m_currentNormalTheme);

  if (m_toggleModeAct) {
    QSignalBlocker blocker(m_toggleModeAct);
    m_toggleModeAct->setChecked(overseerMode);
  }

  setWindowTitle(tr("Lorefarer"));
  setMinimumSize(800, 800);

  QScreen *screen = QGuiApplication::primaryScreen();
  if (screen)
    setGeometry(screen->availableGeometry());
}

void MainWindow::buildNormalPage() {
  m_normalPage = new QWidget(this);

  m_fileWidget = new FileWidget(m_normalPage);
  m_documentManager = new DocumentManager(this);

  m_documentArea = new DocumentArea(m_documentManager, m_normalPage);
  m_documentArea->setEditSession(m_editSession);

  m_chatWidget = new ChatWidget(m_inferenceService, m_editSession,
                                m_normalPage);

  connect(m_chatWidget, &ChatWidget::contextScopesChanged, m_documentArea,
          [this](const QStringList &scopeIds) {
            auto *editor = m_documentArea->currentEditor();
            if (!editor)
              return;
            if (scopeIds.isEmpty())
              editor->clearHighlightedScopes();
            else
              editor->setHighlightedScopes(scopeIds);
          });

  connect(m_chatWidget, &ChatWidget::previewActivationRequested,
          m_documentArea, [this](bool active) {
            auto *tw = m_documentArea->currentTextWidget();
            if (tw)
              tw->activatePreview(active);
          });

  connect(m_documentArea, &DocumentArea::currentEditorChanged, this,
          &MainWindow::bindCurrentEditor, Qt::QueuedConnection);

  connect(m_documentArea, &DocumentArea::openDocumentRequested,
          m_documentManager, &DocumentManager::openFile);

  connect(m_documentArea, &DocumentArea::statusMessage, this,
          [this](const QString &text, int timeoutMs) {
            statusBar()->showMessage(text, timeoutMs);
          });

  connect(m_fileWidget, &FileWidget::fileSelected, m_documentManager,
          &DocumentManager::openFile);

  connect(m_fileWidget, &FileWidget::newNoteRequested, m_documentManager,
          &DocumentManager::newMarkdownFileIn);

  connect(m_fileWidget, &FileWidget::newFolderRequested, m_documentManager,
          &DocumentManager::newFolderIn);

  connect(m_fileWidget, &FileWidget::deleteRequested, m_documentManager,
          &DocumentManager::deleteFile);

  connect(m_fileWidget, &FileWidget::convertToMarkdownRequested,
          m_documentManager, &DocumentManager::convertToMarkdown);

  connect(m_fileWidget, &FileWidget::convertToTextRequested, m_documentManager,
          &DocumentManager::convertToText);

  connect(m_fileWidget, &FileWidget::convertToPlantUmlRequested,
          m_documentManager, &DocumentManager::convertToPlantUml);

  connect(m_fileWidget, &FileWidget::convertToDotRequested, m_documentManager,
          &DocumentManager::convertToDot);

  connect(m_documentManager, &DocumentManager::documentCreated, m_fileWidget,
          &FileWidget::beginEditingPath);

  connect(m_documentManager, &DocumentManager::fileConverted, m_fileWidget,
          &FileWidget::beginEditingPath);

  connect(m_fileWidget, &FileWidget::renameRequested, m_documentManager,
          &DocumentManager::renameFile);

  connect(m_documentManager, &DocumentManager::currentDocumentChanged,
          m_fileWidget, [this](TextDocument *document) {
            m_fileWidget->setActivePath(document ? document->filePath()
                                                 : QString());
            m_fileWidget->setModifiedPaths(modifiedPaths());
          });

  connect(m_documentManager, &DocumentManager::currentDocumentChanged, this,
          [this](TextDocument *document) {
            auto *tw = m_documentArea->currentTextWidget();
            if (!tw)
              return;
            const QString root =
                document && !document->filePath().isEmpty()
                    ? QFileInfo(document->filePath()).absolutePath()
                    : QString();
            tw->setProjectRoot(root);
          });

  connect(m_documentManager, &DocumentManager::currentDocumentChanged, this,
          [this](TextDocument *) {
            auto *editor = m_documentArea->currentEditor();
            if (editor && m_editSession)
              m_editSession->setEditor(editor);
            if (m_chatWidget && editor)
              m_chatWidget->setActiveEditor(editor);
          });

  auto *mainSplitter = new QSplitter(Qt::Horizontal, m_normalPage);
  mainSplitter->addWidget(m_fileWidget);

  auto *rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);
  rightSplitter->addWidget(m_documentArea);
  rightSplitter->addWidget(m_chatWidget);
  rightSplitter->setSizes({650, 300});

  mainSplitter->setSizes({240, 960});

  auto *layout = new QVBoxLayout(m_normalPage);
  layout->setContentsMargins(5, 5, 5, 5);
  layout->addWidget(mainSplitter);
}

void MainWindow::buildOverseerPage() {
  m_overseerPage =
      new OverseerPage(m_inferenceService, m_editSession, this);

  connect(m_overseerPage, &OverseerPage::statusMessage, this,
          [this](const QString &text, int timeoutMs) {
            statusBar()->showMessage(text, timeoutMs);
          });

  connect(m_overseerPage, &OverseerPage::openFileInNormalEditorRequested,
          this, [this](const QString &absolutePath) {
            if (m_documentManager)
              m_documentManager->openFile(absolutePath);

            if (m_centralStack)
              m_centralStack->setCurrentIndex(0);

            if (m_toggleModeAct) {
              QSignalBlocker blocker(m_toggleModeAct);
              m_toggleModeAct->setChecked(false);
            }
          });
}

void MainWindow::bindCurrentEditor(TextEdit *editor) {
  if (!editor)
    return;

  if (m_editSession)
    m_editSession->setEditor(editor);

  if (m_chatWidget)
    m_chatWidget->setActiveEditor(editor);
}

void MainWindow::createActions() {
  auto getSafeIcon = [](const QString &themeIcon,
                        const QString &fallbackPath = "") -> QIcon {
    QIcon icon = QIcon::fromTheme(themeIcon);
    if (icon.isNull() && !fallbackPath.isEmpty())
      icon = QIcon(fallbackPath);
    return icon;
  };

  m_newTextAct =
      new QAction(getSafeIcon("document-new", ":/icons/document-new.png"),
                  tr("&Text File"), this);
  m_newTextAct->setShortcuts(QKeySequence::New);
  connect(m_newTextAct, &QAction::triggered, m_documentManager,
          &DocumentManager::newTextFile);

  m_newMarkdownAct =
      new QAction(getSafeIcon("document-new", ":/icons/document-new.png"),
                  tr("&Markdown File"), this);
  connect(m_newMarkdownAct, &QAction::triggered, m_documentManager,
          &DocumentManager::newMarkdownFile);

  m_newPlantUmlAct =
      new QAction(getSafeIcon("document-new", ":/icons/document-new.png"),
                  tr("&PlantUML Diagram"), this);
  connect(m_newPlantUmlAct, &QAction::triggered, m_documentManager,
          &DocumentManager::newPlantUmlFile);

  m_openAct =
      new QAction(getSafeIcon("document-open", ":/icons/document-open.png"),
                  tr("&Open..."), this);
  m_openAct->setShortcuts(QKeySequence::Open);
  connect(m_openAct, &QAction::triggered, this, [this]() {
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
                                     tr("All Files (*)"));
    if (!path.isEmpty())
      m_documentManager->openFile(path);
  });

  m_saveAct =
      new QAction(getSafeIcon("document-save", ":/icons/document-save.png"),
                  tr("&Save"), this);
  m_saveAct->setShortcuts(QKeySequence::Save);
  connect(m_saveAct, &QAction::triggered, this, [this]() {
    if (m_centralStack->currentIndex() == 1) {
      if (m_overseerPage)
        m_overseerPage->saveAll();
    } else {
      m_documentManager->save();
    }
  });

  m_saveAllAct = new QAction(tr("Save A&ll"), this);
  m_saveAllAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
  connect(m_saveAllAct, &QAction::triggered, this, [this]() {
    if (m_centralStack->currentIndex() == 1) {
      if (m_overseerPage)
        m_overseerPage->saveAll();
    } else {
      for (TextDocument *doc : m_documentManager->openDocuments()) {
        if (doc && doc->isModified())
          m_documentManager->saveDocument(doc);
      }
    }
  });

  m_exitAct = new QAction(
      getSafeIcon("application-exit", ":/icons/application-exit.png"),
      tr("E&xit"), this);
  m_exitAct->setShortcuts(QKeySequence::Quit);
  connect(m_exitAct, &QAction::triggered, this, &QWidget::close);

  m_manageModelsAct = new QAction(tr("&Manage Models..."), this);
  connect(m_manageModelsAct, &QAction::triggered, this,
          &MainWindow::manageModels);

  m_llmSettingsAct = new QAction(tr("LLM &Settings..."), this);
  connect(m_llmSettingsAct, &QAction::triggered, this,
          &MainWindow::openLlmSettings);

  m_toggleModeAct = new QAction(tr("&Overseer Mode"), this);
  m_toggleModeAct->setCheckable(true);
  m_toggleModeAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
  m_toggleModeAct->setStatusTip(tr("Switch between normal and Overseer mode"));
  connect(m_toggleModeAct, &QAction::toggled, this,
          &MainWindow::onModeToggled);

  m_aboutAct = new QAction(getSafeIcon("help-about", ":/icons/help-about.png"),
                           tr("&About"), this);
  connect(m_aboutAct, &QAction::triggered, this, &MainWindow::about);

  m_aboutQtAct = new QAction(tr("About &Qt"), this);
  connect(m_aboutQtAct, &QAction::triggered, this, &MainWindow::aboutQt);

  connect(m_documentManager, &DocumentManager::currentDocumentChanged, this,
          [this](TextDocument *) {
            m_fileWidget->setModifiedPaths(modifiedPaths());
          });
}

void MainWindow::createToolbar() {
  m_topToolBar = addToolBar(tr("Main"));
  m_topToolBar->setObjectName(QStringLiteral("mainToolBar"));
  m_topToolBar->setMovable(false);
  m_topToolBar->setFloatable(false);
  m_topToolBar->setIconSize(QSize(18, 18));
  m_topToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  m_modeButton = new QToolButton(m_topToolBar);
  m_modeButton->setObjectName(QStringLiteral("overseerModeButton"));
  m_modeButton->setDefaultAction(m_toggleModeAct);
  m_modeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_modeButton->setIcon(QIcon::fromTheme(QStringLiteral("view-grid")));
  m_modeButton->setText(tr("Overseer"));

  m_topToolBar->addWidget(m_modeButton);
}

void MainWindow::createMenus() {
  m_fileMenu = menuBar()->addMenu(tr("&File"));

  m_newMenu = m_fileMenu->addMenu(tr("&New"));
  m_newMenu->addAction(m_newTextAct);
  m_newMenu->addAction(m_newMarkdownAct);
  m_newMenu->addAction(m_newPlantUmlAct);

  m_fileMenu->addAction(m_openAct);
  m_fileMenu->addAction(m_saveAct);
  m_fileMenu->addAction(m_saveAllAct);
  m_fileMenu->addSeparator();
  m_fileMenu->addAction(m_exitAct);

  m_viewMenu = menuBar()->addMenu(tr("&View"));
  m_viewMenu->addAction(m_toggleModeAct);

  m_toolsMenu = menuBar()->addMenu(tr("&Tools"));
  m_toolsMenu->addAction(m_llmSettingsAct);
  m_toolsMenu->addSeparator();
  m_toolsMenu->addAction(m_manageModelsAct);

  m_themeMenu = menuBar()->addMenu(tr("&Theme"));

  auto *normalThemeMenu = m_themeMenu->addMenu(tr("Normal"));
  QActionGroup *normalGroup = new QActionGroup(this);
  normalGroup->setExclusive(true);

  for (const QString &theme : ThemeRegistry::instance().names()) {
    QAction *a = normalThemeMenu->addAction(theme);
    a->setCheckable(true);
    normalGroup->addAction(a);

    if (theme == m_currentNormalTheme)
      a->setChecked(true);

    connect(a, &QAction::triggered, this,
            [this, theme]() { onThemeSelected(theme); });
  }

  auto *overseerThemeMenu = m_themeMenu->addMenu(tr("Overseer"));
  QActionGroup *overseerGroup = new QActionGroup(this);
  overseerGroup->setExclusive(true);

  for (const QString &theme : ThemeRegistry::instance().names()) {
    QAction *a = overseerThemeMenu->addAction(theme);
    a->setCheckable(true);
    overseerGroup->addAction(a);

    if (theme == m_currentOverseerTheme)
      a->setChecked(true);

    connect(a, &QAction::triggered, this,
            [this, theme]() { onOverseerThemeSelected(theme); });
  }

  m_helpMenu = menuBar()->addMenu(tr("&Help"));
  m_helpMenu->addAction(m_aboutAct);
  m_helpMenu->addAction(m_aboutQtAct);
}

void MainWindow::applyNormalTheme(const QString &name) {
  const QString resourcePath =
      QString(":/catppuccin-%1/stylesheet.qss").arg(name);

  QFile file(resourcePath);

  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    return;
  }

  const QString qss = QString::fromUtf8(file.readAll());
  file.close();

  m_normalThemeManager->loadTheme(name, qss);

  const ThemeTokens tokens = m_normalThemeManager->currentTokens();

  m_normalPage->setStyleSheet(qss);
  m_normalPage->setPalette(paletteForTokens(tokens));

  if (m_documentArea)
    m_documentArea->setThemeTokens(tokens);

  m_currentNormalTheme = name;

  QSettings settings;
  settings.setValue(NormalThemeKey, name);

  if (m_centralStack && m_centralStack->currentIndex() == 0) {
    ThemeRegistry::instance().setActiveTheme(name);
  }
}

void MainWindow::applyOverseerTheme(const QString &name) {
  const QString resourcePath =
      QString(":/catppuccin-%1/stylesheet.qss").arg(name);

  QFile file(resourcePath);

  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    return;
  }

  const QString qss = QString::fromUtf8(file.readAll());
  file.close();

  m_overseerThemeManager->loadTheme(name, qss);

  const ThemeTokens tokens = m_overseerThemeManager->currentTokens();

  m_overseerPage->setStyleSheet(qss);
  m_overseerPage->setPalette(paletteForTokens(tokens));

  if (m_overseerPage)
    m_overseerPage->setThemeTokens(tokens);

  m_currentOverseerTheme = name;

  QSettings settings;
  settings.setValue(OverseerThemeKey, name);

  if (m_centralStack && m_centralStack->currentIndex() == 1) {
    ThemeRegistry::instance().setActiveTheme(name);
  }
}

QPalette MainWindow::paletteForTokens(const ThemeTokens &tokens) const {
  QPalette pal = QApplication::style()->standardPalette();

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

  return pal;
}

void MainWindow::onModeToggled(bool overseerMode) {
  const int targetIndex = overseerMode ? 1 : 0;

  if (m_centralStack->currentIndex() == targetIndex)
    return;

  if (targetIndex == 1) {
    if (!confirmDiscardChanges(tr("Normal mode"))) {
      QSignalBlocker blocker(m_toggleModeAct);
      m_toggleModeAct->setChecked(false);
      return;
    }
  } else {
    if (m_overseerPage && !confirmDiscardChanges(tr("Overseer mode"))) {
      QSignalBlocker blocker(m_toggleModeAct);
      m_toggleModeAct->setChecked(true);
      return;
    }
  }

  m_centralStack->setCurrentIndex(targetIndex);

  ThemeRegistry::instance().setActiveTheme(
      overseerMode ? m_currentOverseerTheme : m_currentNormalTheme);

  if (m_modeButton) {
    m_modeButton->setText(overseerMode ? tr("Normal") : tr("Overseer"));
    m_modeButton->setIcon(
        QIcon::fromTheme(overseerMode ? QStringLiteral("go-home")
                                      : QStringLiteral("view-grid")));
  }

  QSettings settings;
  settings.setValue(ModeKey, overseerMode);
}

bool MainWindow::confirmDiscardChanges(const QString &areaName) {
  DocumentManager *manager = nullptr;

  if (areaName == tr("Normal mode"))
    manager = m_documentManager;
  else if (m_overseerPage)
    manager = m_overseerPage->documentManager();

  if (!manager)
    return true;

  QList<TextDocument *> dirty;

  for (TextDocument *doc : manager->openDocuments()) {
    if (doc && doc->isModified())
      dirty.append(doc);
  }

  if (dirty.isEmpty())
    return true;

  QMessageBox box(this);
  box.setIcon(QMessageBox::Question);
  box.setWindowTitle(tr("Unsaved changes"));
  box.setText(tr("%n file(s) have unsaved changes.", "", dirty.size()));
  box.setInformativeText(tr("Save before switching modes?"));
  box.setStandardButtons(QMessageBox::SaveAll | QMessageBox::Discard |
                         QMessageBox::Cancel);
  box.setDefaultButton(QMessageBox::SaveAll);

  const int result = box.exec();

  if (result == QMessageBox::Cancel)
    return false;

  if (result == QMessageBox::Discard) {
    for (TextDocument *doc : std::as_const(dirty))
      doc->setModified(false);
    return true;
  }

  for (TextDocument *doc : std::as_const(dirty)) {
    if (!manager->saveDocument(doc))
      return false;
  }

  return true;
}

void MainWindow::onThemeSelected(const QString &theme) {
  applyNormalTheme(theme);
}

void MainWindow::onOverseerThemeSelected(const QString &theme) {
  applyOverseerTheme(theme);
}

void MainWindow::openLlmSettings() {
  if (!m_llmSettingsPanel) {
    m_llmSettingsPanel = new LlmSettingsPanel(m_inferenceService, this);
  }
  m_llmSettingsPanel->show();
  m_llmSettingsPanel->raise();
  m_llmSettingsPanel->activateWindow();
}

void MainWindow::manageModels() {
  if (!m_modelDialog) {
    m_modelDialog = new ModelDialog(m_inferenceService, this);
  }
  m_modelDialog->show();
  m_modelDialog->raise();
  m_modelDialog->activateWindow();
}

QSet<QString> MainWindow::modifiedPaths() const {
  QSet<QString> paths;

  if (!m_documentManager)
    return paths;

  for (TextDocument *doc : m_documentManager->openDocuments()) {
    if (doc && doc->isModified() && !doc->filePath().isEmpty())
      paths.insert(doc->filePath());
  }

  return paths;
}

void MainWindow::about() {
  QMessageBox::about(this, tr("About Lorefarer"),
                     tr("The <b>Lorefarer</b> document editor."));
}

void MainWindow::aboutQt() { QMessageBox::aboutQt(this, tr("About Qt")); }

void MainWindow::closeEvent(QCloseEvent *event) {
  if (!confirmDiscardChanges(tr("Normal mode"))) {
    event->ignore();
    return;
  }

  if (m_overseerPage && m_overseerPage->hasUnsavedChanges()) {
    if (!confirmDiscardChanges(tr("Overseer mode"))) {
      event->ignore();
      return;
    }
  }

  QSettings settings;
  settings.setValue(ModeKey, m_centralStack->currentIndex() == 1);

  event->accept();
}