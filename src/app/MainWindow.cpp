#include "MainWindow.h"
#include "../../include/ingest/Extractors.h"
#include "../../include/ingest/IngestRegistry.h"
#include "../../include/ingest/IngestService.h"
#include "../../include/ingest/NoteWriter.h"
#include "../../include/voice/DictateCommand.h"
#include "../../include/voice/ReadAloudCommand.h"
#include "../../include/voice/SpeechController.h"
#include "../../include/voice/SpeechPanel.h"
#include "../../include/voice/VoiceCommandRegistry.h"
#include "ChatWidget.h"
#include "DocumentArea.h"
#include "EditSession.h"
#include "FileWidget.h"
#include "LiveDictateCommand.h"
#include "LlmSettingsPanel.h"
#include "NotificationService.h"
#include "OverseerPage.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "TextEdit.h"
#include "TextWidget.h"
#include "ThemeRegistry.h"
#include "ThemeTokens.h"
#include "ToastStack.h"
#include "app/QfPaths.h"
#include "inference/InferenceService.h"
#include "ui/ModelDialog.h"

#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
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
#include <QProgressDialog>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QThreadPool>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr auto NormalThemeKey = "theme";
constexpr auto OverseerThemeKey = "overseer/theme";
constexpr auto ModeKey = "overseer/mode";

constexpr int ImportConcurrency = 4;

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

QString readResourceStylesheet(const QString &themeName) {
  QFile file(QStringLiteral(":/themes/%1/stylesheet.qss").arg(themeName));

  if (!file.open(QFile::ReadOnly | QFile::Text))
    return {};

  const QString qss = QString::fromUtf8(file.readAll());
  file.close();

  return qss;
}

QString expandTokens(const QString &qss,
                     const QHash<QString, QColor> &tokens) {
  QString out = qss;

  QStringList keys = tokens.keys();
  std::sort(keys.begin(), keys.end(),
            [](const QString &a, const QString &b) {
              return a.size() > b.size();
            });

  for (const QString &key : std::as_const(keys)) {
    const QColor color = tokens.value(key);

    if (!color.isValid())
      continue;

    const QString needle = QStringLiteral("@") + key;
    const QString replacement = color.name(QColor::HexRgb);

    out.replace(needle, replacement);
  }

  return out;
}

} // namespace

MainWindow::~MainWindow() = default;

MainWindow::MainWindow() {
  setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  m_normalThemeManager = new ThemeManager(this);
  m_overseerThemeManager = new ThemeManager(this);

  m_inferenceService = new InferenceService(this);

  m_inferenceService->initialize(
    LlamaManager::Backend::Vulkan, QString(),
    InferenceService::SttModel::Nemotron35, configuredLlm());

  m_editSession = new EditSession(nullptr, this);

  if (!loadAllThemes()) {
    QMessageBox::critical(
        this, tr("Theme load failure"),
        tr("Lore could not load its base theme. The application "
           "cannot start without a valid theme stylesheet."));
    std::exit(1);
  }

  buildNormalPage();
  buildOverseerPage();
  buildIngestLayer();
  buildSpeechLayer();

  m_centralStack = new QStackedWidget(this);
  m_centralStack->addWidget(m_normalPage);
  m_centralStack->addWidget(m_overseerPage);

  setCentralWidget(m_centralStack);

  m_toastStack = new ToastStack(this);
  NotificationService::instance().setToastHost(m_toastStack);

  createActions();
  createToolbar();
  createMenus();

  QSettings settings;

  m_currentNormalTheme =
      settings.value(NormalThemeKey,
                     ThemeRegistry::instance().defaultNormalName())
          .toString();

  if (!ThemeRegistry::instance().contains(m_currentNormalTheme))
    m_currentNormalTheme = ThemeRegistry::instance().defaultNormalName();

  m_currentOverseerTheme =
      settings.value(OverseerThemeKey, QString()).toString();

  if (!ThemeRegistry::instance().contains(m_currentOverseerTheme))
    m_currentOverseerTheme = ThemeRegistry::instance().defaultOverseerName();

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

  setWindowTitle(tr("Lore"));
  setMinimumSize(800, 800);

  QScreen *screen = QGuiApplication::primaryScreen();
  if (screen)
    setGeometry(screen->availableGeometry());
}

bool MainWindow::loadThemeFromResource(const QString &name) {
  const QString qss = readResourceStylesheet(name);

  if (qss.isEmpty()) {
    qWarning() << "[MainWindow] Stylesheet not found or empty:" << name;
    return false;
  }

  QStringList missing;

  if (!ThemeRegistry::instance().registerFromStylesheet(name, qss,
                                                        &missing)) {
    if (!missing.isEmpty()) {
      qWarning() << "[MainWindow] Theme" << name
                 << "is missing tokens:" << missing;
    } else {
      qWarning() << "[MainWindow] Theme" << name
                 << "failed to register. See [ThemeRegistry] log lines.";
    }
    return false;
  }

  return true;
}

bool MainWindow::loadAllThemes() {
  const bool loreOk =
      loadThemeFromResource(ThemeRegistry::instance().baseName());

  if (!loreOk) {
    qCritical() << "[MainWindow] Base theme 'lore' could not be loaded. "
                   "The application cannot start.";
    return false;
  }

  const QStringList candidates = {
      ThemeRegistry::instance().defaultNormalName(),
      ThemeRegistry::instance().defaultOverseerName(),
  };

  for (const QString &name : candidates) {
    if (name == ThemeRegistry::instance().baseName())
      continue;

    if (!loadThemeFromResource(name)) {
      qWarning() << "[MainWindow] Theme" << name
                 << "did not register; it will not appear in the menu.";
    }
  }

  return true;
}

QString MainWindow::combinedStylesheet(const QString &themeName) const {
  const QString baseQss =
      readResourceStylesheet(ThemeRegistry::instance().baseName());

  if (baseQss.isEmpty()) {
    qWarning() << "[MainWindow] Base stylesheet missing.";
    return {};
  }

  const ThemeTokens tokens = ThemeRegistry::instance().tokens(themeName);

  QHash<QString, QColor> tokenMap;

  auto add = [&tokenMap](const QString &name, const QColor &color) {
    if (color.isValid())
      tokenMap.insert(name, color);
  };

  add(QStringLiteral("base"), tokens.base);
  add(QStringLiteral("surface0"), tokens.surface0);
  add(QStringLiteral("surface1"), tokens.surface1);
  add(QStringLiteral("surface2"), tokens.surface2);
  add(QStringLiteral("surface-raised"), tokens.surfaceRaised);
  add(QStringLiteral("structure"), tokens.structure);

  add(QStringLiteral("text"), tokens.text);
  add(QStringLiteral("text-muted"), tokens.textMuted);
  add(QStringLiteral("text-subtle"), tokens.textSubtle);
  add(QStringLiteral("text-disabled"), tokens.textDisabled);

  add(QStringLiteral("accent"), tokens.accent);
  add(QStringLiteral("accent-hover"), tokens.accentHover);
  add(QStringLiteral("accent-pressed"), tokens.accentPressed);
  add(QStringLiteral("accent-muted"), tokens.accentMuted);
  add(QStringLiteral("accent-fg"), tokens.accentFg);

  add(QStringLiteral("hint-cool"), tokens.hintCool);
  add(QStringLiteral("hint-warm"), tokens.hintWarm);
  add(QStringLiteral("hint-neutral"), tokens.hintNeutral);

  add(QStringLiteral("border"), tokens.border);
  add(QStringLiteral("border-strong"), tokens.borderStrong);
  add(QStringLiteral("divider"), tokens.divider);

  add(QStringLiteral("success"), tokens.success);
  add(QStringLiteral("warning"), tokens.warning);
  add(QStringLiteral("error"), tokens.error);
  add(QStringLiteral("info"), tokens.info);

  QString concatenated = baseQss;

  if (themeName != ThemeRegistry::instance().baseName()) {
    const QString themeQss = readResourceStylesheet(themeName);

    if (!themeQss.isEmpty()) {
      concatenated += QStringLiteral("\n\n/* ---- theme: ");
      concatenated += themeName;
      concatenated += QStringLiteral(" ---- */\n\n");
      concatenated += themeQss;
    }
  }

  return expandTokens(concatenated, tokenMap);
}

void MainWindow::applyNormalTheme(const QString &name) {
  const ThemeTokens tokens = ThemeRegistry::instance().tokens(name);

  if (!tokens.isComplete()) {
    qWarning() << "[MainWindow] Refusing to apply theme" << name
               << "— its tokens are not complete.";
    return;
  }

  const QString combined = combinedStylesheet(name);

  if (combined.isEmpty())
    return;

  m_normalThemeManager->loadTheme(name, combined);

  m_normalPage->setStyleSheet(combined);
  m_normalPage->setPalette(paletteForTokens(tokens));

  if (m_documentArea)
    m_documentArea->setThemeTokens(tokens);

  m_currentNormalTheme = name;

  QSettings settings;
  settings.setValue(NormalThemeKey, name);

  if (m_centralStack && m_centralStack->currentIndex() == 0)
    ThemeRegistry::instance().setActiveTheme(name);
}

void MainWindow::applyOverseerTheme(const QString &name) {
  const ThemeTokens tokens = ThemeRegistry::instance().tokens(name);

  if (!tokens.isComplete()) {
    qWarning() << "[MainWindow] Refusing to apply theme" << name
               << "— its tokens are not complete.";
    return;
  }

  const QString combined = combinedStylesheet(name);

  if (combined.isEmpty())
    return;

  m_overseerThemeManager->loadTheme(name, combined);

  m_overseerPage->setStyleSheet(combined);
  m_overseerPage->setPalette(paletteForTokens(tokens));

  if (m_overseerPage)
    m_overseerPage->setThemeTokens(tokens);

  m_currentOverseerTheme = name;

  QSettings settings;
  settings.setValue(OverseerThemeKey, name);

  if (m_centralStack && m_centralStack->currentIndex() == 1)
    ThemeRegistry::instance().setActiveTheme(name);
}

QPalette MainWindow::paletteForTokens(const ThemeTokens &tokens) const {
  QPalette pal = QApplication::style()->standardPalette();

  pal.setColor(QPalette::Window, tokens.base);
  pal.setColor(QPalette::WindowText, tokens.text);
  pal.setColor(QPalette::Base, tokens.surface0);
  pal.setColor(QPalette::AlternateBase, tokens.surfaceRaised);
  pal.setColor(QPalette::Text, tokens.text);
  pal.setColor(QPalette::PlaceholderText, tokens.textSubtle);
  pal.setColor(QPalette::Button, tokens.surface0);
  pal.setColor(QPalette::ButtonText, tokens.text);
  pal.setColor(QPalette::BrightText, tokens.error);
  pal.setColor(QPalette::Highlight, tokens.accent);
  pal.setColor(QPalette::HighlightedText, tokens.accentFg);
  pal.setColor(QPalette::Link, tokens.accent);
  pal.setColor(QPalette::LinkVisited, tokens.accentMuted);
  pal.setColor(QPalette::ToolTipBase, tokens.surfaceRaised);
  pal.setColor(QPalette::ToolTipText, tokens.text);
  pal.setColor(QPalette::Light, tokens.surface2);
  pal.setColor(QPalette::Midlight, tokens.surface1);
  pal.setColor(QPalette::Dark, tokens.structure);
  pal.setColor(QPalette::Mid, tokens.border);
  pal.setColor(QPalette::Shadow, tokens.base);

  return pal;
}

void MainWindow::buildNormalPage() {
  m_normalPage = new QWidget(this);

  m_fileWidget = new FileWidget(m_normalPage);
  m_documentManager = new DocumentManager(this);

  m_documentArea = new DocumentArea(m_documentManager, m_normalPage);
  m_documentArea->setEditSession(m_editSession);

  m_chatWidget =
      new ChatWidget(m_inferenceService, m_editSession, m_normalPage);

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

  m_importFilesAct =
      new QAction(getSafeIcon("document-import",
                              ":/icons/document-import.png"),
                  tr("Import &Files..."), this);
  m_importFilesAct->setStatusTip(
      tr("Convert documents into notes in the notes folder"));
  connect(m_importFilesAct, &QAction::triggered, this,
          &MainWindow::onImportFilesDialog);

  m_importFolderAct =
      new QAction(getSafeIcon("document-import",
                              ":/icons/document-import.png"),
                  tr("Import F&older..."), this);
  m_importFolderAct->setStatusTip(
      tr("Recursively convert every supported document in a folder"));
  connect(m_importFolderAct, &QAction::triggered, this,
          &MainWindow::onImportFolderDialog);

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

  m_settingsAct = new QAction(tr("&Settings..."), this);
  connect(m_settingsAct, &QAction::triggered, this,
          &MainWindow::openSettings);

  m_toggleModeAct = new QAction(tr("&Overseer Mode"), this);
  m_toggleModeAct->setCheckable(true);
  m_toggleModeAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
  m_toggleModeAct->setStatusTip(tr("Switch between normal and Overseer mode"));
  connect(m_toggleModeAct, &QAction::toggled, this,
          &MainWindow::onModeToggled);

  m_toggleSpeechAct = new QAction(tr("Voice Panel"), this);
  m_toggleSpeechAct->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space));
  m_toggleSpeechAct->setStatusTip(
      tr("Show or hide the voice command panel"));
  connect(m_toggleSpeechAct, &QAction::triggered, this,
          &MainWindow::onToggleSpeechPanel);

  m_aboutAct = new QAction(getSafeIcon("help-about", ":/icons/help-about.png"),
                           tr("&About"), this);
  connect(m_aboutAct, &QAction::triggered, this, &MainWindow::about);

  m_aboutQtAct = new QAction(tr("About &Qt"), this);
  connect(m_aboutQtAct, &QAction::triggered, this, &MainWindow::aboutQt);

  addAction(m_toggleSpeechAct);
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
  m_fileMenu->addSeparator();
  m_fileMenu->addAction(m_importFilesAct);
  m_fileMenu->addAction(m_importFolderAct);
  m_fileMenu->addSeparator();
  m_fileMenu->addAction(m_saveAct);
  m_fileMenu->addAction(m_saveAllAct);
  m_fileMenu->addSeparator();
  m_fileMenu->addAction(m_exitAct);

  m_viewMenu = menuBar()->addMenu(tr("&View"));
  m_viewMenu->addAction(m_toggleModeAct);
  m_viewMenu->addSeparator();
  m_viewMenu->addAction(m_toggleSpeechAct);

  m_toolsMenu = menuBar()->addMenu(tr("&Tools"));
  m_toolsMenu->addAction(m_settingsAct);
  m_toolsMenu->addSeparator();
  m_toolsMenu->addAction(m_llmSettingsAct);
  m_toolsMenu->addSeparator();
  m_toolsMenu->addAction(m_manageModelsAct);

  m_themeMenu = menuBar()->addMenu(tr("&Theme"));

  auto *normalThemeMenu = m_themeMenu->addMenu(tr("Normal"));
  QActionGroup *normalGroup = new QActionGroup(this);
  normalGroup->setExclusive(true);

  for (const QString &theme : ThemeRegistry::instance().selectableNames()) {
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

  for (const QString &theme : ThemeRegistry::instance().selectableNames()) {
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

void MainWindow::openSettings() {
  SettingsDialog dialog(this);
  dialog.exec();
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
  QMessageBox::about(this, tr("About Lore"),
                     tr("The <b>Lore</b> document editor."));
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

void MainWindow::buildIngestLayer() {
  QThreadPool::globalInstance()->setMaxThreadCount(ImportConcurrency);

  m_ingestRegistry = std::make_unique<IngestRegistry>();
  registerBuiltinExtractors(*m_ingestRegistry);

  m_noteWriter = std::make_unique<DiskNoteWriter>();

  m_ingestService =
      new IngestService(m_ingestRegistry.get(), m_noteWriter.get(), this);

  m_ingestService->setMaxConcurrent(ImportConcurrency);

  QDir().mkpath(notesRootPath());

  const QStringList importable = m_ingestService->importableExtensions();

  if (m_fileWidget) {
    m_fileWidget->setImportableExtensions(importable);

    connect(m_fileWidget, &FileWidget::importRequested, this,
            &MainWindow::onImportRequested);
    connect(m_fileWidget, &FileWidget::importAllRequested, this,
            &MainWindow::onImportAllRequested);
  }

  if (m_chatWidget) {
    connect(m_chatWidget, &ChatWidget::importRequested, this,
            &MainWindow::onImportRequested);
    connect(m_chatWidget, &ChatWidget::importAllRequested, this,
            &MainWindow::onImportAllRequested);
  }
}

QString MainWindow::notesRootPath() const {
  return QStandardPaths::writableLocation(
             QStandardPaths::AppDataLocation) +
         QStringLiteral("/notes");
}

QString MainWindow::importDialogFilter() const {
  if (!m_ingestService) {
    return tr("All Files (*)");
  }

  QStringList patterns;
  for (const QString &ext : m_ingestService->importableExtensions()) {
    patterns << QStringLiteral("*.") + ext;
  }
  if (patterns.isEmpty()) {
    return tr("All Files (*)");
  }
  return tr("Importable Documents (%1);;All Files (*)")
      .arg(patterns.join(QLatin1Char(' ')));
}

QStringList MainWindow::filterImportable(const QStringList &paths) const {
  QStringList result;
  if (!m_ingestService) {
    return result;
  }
  for (const QString &path : paths) {
    if (m_ingestService->canImport(path)) {
      result.append(path);
    }
  }
  return result;
}

QStringList MainWindow::collectImportableFilesIn(
    const QString &folderPath) const {
  QStringList result;
  if (folderPath.isEmpty() || !m_ingestService) {
    return result;
  }

  QStringList patterns;
  for (const QString &ext : m_ingestService->importableExtensions()) {
    patterns << QStringLiteral("*.") + ext;
  }
  if (patterns.isEmpty()) {
    return result;
  }

  QDirIterator it(folderPath, patterns, QDir::Files | QDir::Readable,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    result.append(it.next());
  }

  result.sort(Qt::CaseInsensitive);
  return result;
}

void MainWindow::onImportFilesDialog() {
  if (!m_ingestService) {
    return;
  }

  const QStringList chosen = QFileDialog::getOpenFileNames(
      this, tr("Import Files"), QString(), importDialogFilter());

  if (chosen.isEmpty()) {
    return;
  }

  const QStringList paths = filterImportable(chosen);
  if (paths.isEmpty()) {
    return;
  }

  onImportAllRequested(paths);
}

void MainWindow::onImportFolderDialog() {
  if (!m_ingestService) {
    return;
  }

  const QString folder = QFileDialog::getExistingDirectory(
      this, tr("Import Folder"), QString(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (folder.isEmpty()) {
    return;
  }

  const QStringList files = collectImportableFilesIn(folder);

  if (files.isEmpty()) {
    reportImportSummary(0, 0, 0);
    return;
  }

  onImportAllRequested(files);
}

void MainWindow::onImportRequested(const QString &path) {
  importOne(path);
}

void MainWindow::onImportAllRequested(const QStringList &paths) {
  if (paths.isEmpty() || !m_ingestService) {
    return;
  }

  startBulkImport(paths);
}

void MainWindow::startBulkImport(const QStringList &paths) {
  m_bulkImportSucceeded = 0;
  m_bulkImportFailed = 0;
  m_bulkImportTotal = paths.size();
  m_bulkImportCompleted = 0;
  m_bulkImportCancelled = false;
  m_bulkImportTokens.clear();

  m_importProgress = new QProgressDialog(
      tr("Importing %1 file(s)…").arg(paths.size()), tr("Cancel"), 0,
      paths.size(), this);
  m_importProgress->setWindowTitle(tr("Import"));
  m_importProgress->setWindowModality(Qt::WindowModal);
  m_importProgress->setMinimumDuration(0);
  m_importProgress->setAutoClose(false);
  m_importProgress->setAutoReset(false);
  m_importProgress->show();

  connect(m_importProgress, &QProgressDialog::canceled, this,
          &MainWindow::onBulkImportCancelled);

  for (const QString &path : paths) {
    IngestOptions options;
    options.destinationFolder = notesRootPath();
    options.writeProvenance = true;
    options.sectionPerPage = true;

    const quint64 token = m_ingestService->import(
        path, options,
        [this, token](IngestService::Outcome outcome) {
          onBulkImportCompleted(token, outcome.ok());
          if (outcome.ok() && m_documentManager) {
            m_documentManager->openFile(outcome.notePath);
          } else if (!outcome.ok() && !m_bulkImportCancelled) {
            reportImportFailure(QString(), outcome.error);
          }
        });

    if (token != 0) {
      m_bulkImportTokens.append(token);
    } else {
      onBulkImportCompleted(0, false);
    }
  }
}

void MainWindow::onBulkImportCompleted(quint64, bool ok) {
  if (ok) {
    ++m_bulkImportSucceeded;
  } else {
    ++m_bulkImportFailed;
  }

  ++m_bulkImportCompleted;

  if (m_importProgress) {
    m_importProgress->setValue(m_bulkImportCompleted);
  }

  if (m_bulkImportCompleted >= m_bulkImportTotal) {
    finishBulkImport();
  }
}

void MainWindow::onBulkImportCancelled() {
  if (m_bulkImportCancelled) {
    return;
  }
  m_bulkImportCancelled = true;

  if (m_ingestService) {
    for (quint64 token : std::as_const(m_bulkImportTokens)) {
      m_ingestService->cancel(token);
    }
  }
  m_bulkImportTokens.clear();

  finishBulkImport();
}

void MainWindow::finishBulkImport() {
  if (m_importProgress) {
    m_importProgress->close();
    m_importProgress->deleteLater();
    m_importProgress = nullptr;
  }

  m_bulkImportTokens.clear();

  if (m_bulkImportCancelled) {
    const int completed = m_bulkImportSucceeded + m_bulkImportFailed;
    NotificationService::instance().warning(
        tr("Import cancelled"),
        tr("Stopped after %1 of %2 file(s).")
            .arg(completed)
            .arg(m_bulkImportTotal));
  } else {
    reportImportSummary(m_bulkImportSucceeded, m_bulkImportFailed,
                        m_bulkImportTotal);
  }
}

void MainWindow::importOne(const QString &sourcePath) {
  if (!m_ingestService) {
    return;
  }

  IngestOptions options;
  options.destinationFolder = notesRootPath();
  options.writeProvenance = true;
  options.sectionPerPage = true;

  m_ingestService->import(
      sourcePath, options,
      [this, sourcePath](IngestService::Outcome outcome) {
        if (!outcome.ok()) {
          reportImportFailure(sourcePath, outcome.error);
          return;
        }
        if (m_documentManager) {
          m_documentManager->openFile(outcome.notePath);
        }
      });
}

void MainWindow::reportImportFailure(const QString &sourcePath,
                                     const QString &error) {
  const QString title = sourcePath.isEmpty()
                            ? tr("Import failed")
                            : tr("Import failed: %1")
                                  .arg(QFileInfo(sourcePath).fileName());

  NotificationService::instance().error(
      title, error, sourcePath.isEmpty() ? QString() : sourcePath);
}

void MainWindow::reportImportSummary(int succeeded, int failed, int total) {
  if (total == 0) {
    NotificationService::instance().info(
        tr("Import"),
        tr("No importable documents were found in that folder."));
    return;
  }

  if (failed == 0) {
    NotificationService::instance().info(
        tr("Import complete"),
        tr("%n note(s) created.", "", succeeded));
    return;
  }

  if (succeeded == 0) {
    NotificationService::instance().error(
        tr("Import failed"),
        tr("None of the %1 document(s) could be imported.").arg(total));
    return;
  }

  NotificationService::instance().warning(
      tr("Import partly complete"),
      tr("%1 of %2 imported; %3 failed.")
          .arg(succeeded)
          .arg(total)
          .arg(failed));
}

void MainWindow::buildSpeechLayer() {
  m_speechController = new SpeechController(m_inferenceService, this);

  if (m_inferenceService) {
    m_inferenceService->setTtsEnabled(true);
  }

  m_voiceCommands = new VoiceCommandRegistry(this);
  m_voiceCommands->add(
      std::make_unique<DictateCommand>(m_speechController));
  m_voiceCommands->add(
      std::make_unique<LiveDictateCommand>(m_speechController));
  m_voiceCommands->add(
      std::make_unique<ReadAloudCommand>(m_speechController));

  m_speechPanel =
      new SpeechPanel(m_voiceCommands, m_speechController, this);
  m_speechPanel->hide();

  if (m_documentManager) {
    connect(m_documentManager, &DocumentManager::currentDocumentChanged, this,
            [this](TextDocument *) {
              onCurrentEditorChangedForSpeech(
                  m_documentArea ? m_documentArea->currentEditor() : nullptr);
            });

    onCurrentEditorChangedForSpeech(
        m_documentArea ? m_documentArea->currentEditor() : nullptr);
  }
}
void MainWindow::onToggleSpeechPanel() {
  if (!m_speechPanel) {
    return;
  }

  if (m_speechPanel->isVisible()) {
    m_speechPanel->hide();
  } else {
    m_speechPanel->show();
    m_speechPanel->raise();
    m_speechPanel->activateWindow();
  }
}

void MainWindow::onCurrentEditorChangedForSpeech(TextEdit *editor) {
  m_currentSpeechEditor = editor;

  if (!m_speechPanel) {
    return;
  }

  VoiceContext context;
  context.editor = editor;
  context.inference = m_inferenceService;
  m_speechPanel->setContext(context);
}