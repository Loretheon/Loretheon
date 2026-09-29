#include "MainWindow.h"

#include "../../include/assistant/LoreAssistant.h"
#include "../../include/assistant/AssistantShell.h"
#include "../../include/avatar/AvatarConfig.h"
#include "../../include/avatar/AvatarWidget.h"
#include "../../include/file/FileSystemView.h"
#include "../../include/ingest/Extractors.h"
#include "../../include/ingest/IngestRegistry.h"
#include "../../include/ingest/IngestService.h"
#include "../../include/ingest/NoteWriter.h"
#include "../../include/search/NotePromoter.h"
#include "../../include/search/RetrievalLoop.h"
#include "../../include/search/ScopeIndex.h"
#include "../../include/search/SearchPage.h"
#include "../../include/search/SearchService.h"
#include "../../include/text/LoreInputDialog.h"
#include "../../include/text/LoreTrigger.h"
#include "../../include/voice/DictateCommand.h"
#include "../../include/voice/LiveDictateCommand.h"
#include "../../include/voice/ReadAloudCommand.h"
#include "../../include/voice/SpeechController.h"
#include "../../include/voice/SpeechPanel.h"
#include "../../include/voice/VoiceCommandRegistry.h"
#include "AutoHideDock.h"
#include "ChatWidget.h"
#include "CustomTitleBar.h"
#include "DockReservation.h"
#include "DocumentArea.h"
#include "EditSession.h"
#include "FileWidget.h"
#include "LlmSettingsPanel.h"
#include "NotificationService.h"
#include "OverseerPage.h"
#include "OverseerSessionManager.h"
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
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QPalette>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextCursor>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {

constexpr auto NormalThemeKey = "theme";
constexpr auto OverseerThemeKey = "overseer/theme";
constexpr auto ModeKey = "ui/mode";

constexpr int ImportConcurrency = 4;

const AvatarConfig kAvatarConfig{};

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

MainWindow::~MainWindow() {
  if (m_assistant) {
    m_assistant->stop();
  }
}

MainWindow::MainWindow() {
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
  setAttribute(Qt::WA_TranslucentBackground, false);

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

  m_editSession->setInferenceService(m_inferenceService);

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
  buildSearchLayer();

  // ---- workspace: title bar + the three working modes ----

  m_centralStack = new QStackedWidget(this);
  m_centralStack->addWidget(m_normalPage);
  m_centralStack->addWidget(m_overseerPage);
  m_centralStack->addWidget(m_searchPage);

  m_workspacePage = new QWidget(this);
  auto *workspaceLayout = new QVBoxLayout(m_workspacePage);
  workspaceLayout->setContentsMargins(0, 0, 0, 0);
  workspaceLayout->setSpacing(0);

  createCustomTitleBar();
  workspaceLayout->addWidget(m_titleBar);
  workspaceLayout->addWidget(m_centralStack, 1);

  // ---- assistant shell: avatar, prompt, no chrome ----

  buildAssistantShell();

  // ---- the top-level shell stack ----

  m_shellStack = new QStackedWidget(this);
  m_shellStack->addWidget(m_assistantShellPage);
  m_shellStack->addWidget(m_workspacePage);

  setCentralWidget(m_shellStack);

  m_toastStack = new ToastStack(this);
  NotificationService::instance().setToastHost(m_toastStack);

  wireTitleBar();

  {
    LoreAssistant::Config config;
    config.inference = m_inferenceService;
    config.avatar = m_avatar;
    config.documents = m_documentManager;
    config.documentArea = m_documentArea;
    config.search = m_searchService;
    config.overseerManager = m_overseerSessionManager;
    config.promoter = m_notePromoter;
    config.scopeIndex = m_scopeIndex.get();
    config.notesRoot = notesRootPath();
    config.root = QStandardPaths::writableLocation(
                      QStandardPaths::AppDataLocation) +
                  QStringLiteral("/assistant");

    m_assistant = new LoreAssistant(config, this);
    m_assistant->start();

    m_assistantShell->setAssistant(m_assistant);
    m_assistantShell->setSpeechController(m_speechController);

    connect(m_assistantShell, &AssistantShell::messageSubmitted,
            this, &MainWindow::onAssistantMessageSubmitted);

    connect(m_assistantShell, &AssistantShell::dismissed,
            this, &MainWindow::leaveAssistantShell);
  }

  connect(m_inferenceService, &InferenceService::ttsReady, this,
          [this]() {
            if (m_assistant) {
              m_assistant->say(QStringLiteral("hello welcome to Lore"));
            }
          },
          Qt::SingleShotConnection);

  connect(m_documentManager, &DocumentManager::documentSaved, this,
          &MainWindow::onDocumentSaved);

  connect(m_documentManager, &DocumentManager::fileDeleted, this,
          [this](const QString &path) {
            if (m_scopeIndex) {
              m_scopeIndex->removeFile(path);
            }
          });

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

  const int storedMode = settings.value(ModeKey, 0).toInt();

  switch (storedMode) {
  case 1:
    setMode(Mode::Overseer);
    break;
  case 2:
    setMode(Mode::Search);
    break;
  default:
    setMode(Mode::Normal);
    break;
  }

  applyNormalTheme(m_currentNormalTheme);
  applyOverseerTheme(m_currentOverseerTheme);

  setWindowTitle(tr("Lore"));
  setMinimumSize(800, 800);

  QScreen *screen = QGuiApplication::primaryScreen();
  if (screen)
    setGeometry(screen->availableGeometry());

  setShell(Shell::Assistant);

  m_escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  m_escapeShortcut->setContext(Qt::ApplicationShortcut);
  connect(m_escapeShortcut, &QShortcut::activated, this,
          &MainWindow::leaveAssistantShell);

  m_summonShortcut = new QShortcut(
      QKeySequence(Qt::CTRL | Qt::Key_Space), this);
  m_summonShortcut->setContext(Qt::ApplicationShortcut);
  connect(m_summonShortcut, &QShortcut::activated, this,
          &MainWindow::enterAssistantShell);
}

void MainWindow::buildAssistantShell() {
  m_assistantShell = new AssistantShell(this);

  m_avatar = new AvatarWidget(m_assistantShell);
  m_avatar->applyConfig(kAvatarConfig);
  m_avatar->setModel(QStringLiteral("qrc:/avatar/julia/julia.glb"));

  m_assistantShell->setAvatar(m_avatar);
  m_assistantShell->setOverseerManager(m_overseerSessionManager);

  m_assistantShellPage = m_assistantShell;
}
void MainWindow::setShell(Shell shell) {
  if (!m_shellStack)
    return;

  m_shell = shell;
  m_shellStack->setCurrentIndex(static_cast<int>(shell));

  if (shell == Shell::Assistant) {
    if (m_assistantShell)
      m_assistantShell->focusPrompt();
  }
}

void MainWindow::enterAssistantShell() {
  setShell(Shell::Assistant);
}

void MainWindow::leaveAssistantShell() {
  setShell(Shell::Workspace);
}

void MainWindow::createCustomTitleBar() {
  m_titleBar = new CustomTitleBar(this);

  connect(m_titleBar, &CustomTitleBar::minimizeRequested, this,
          &QWidget::showMinimized);

  connect(m_titleBar, &CustomTitleBar::maximizeRequested, this, [this]() {
    if (isMaximized()) {
      showNormal();
    } else {
      showMaximized();
    }
  });

  connect(m_titleBar, &CustomTitleBar::closeRequested, this,
          &QWidget::close);
}

void MainWindow::wireTitleBar() {
  CustomTitleBar::Callbacks cb;

  cb.newText     = [this]() { m_documentManager->newTextFile(); };
  cb.newMarkdown = [this]() { m_documentManager->newMarkdownFile(); };
  cb.newPlantUml = [this]() { m_documentManager->newPlantUmlFile(); };

  cb.open = [this]() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open File"), QString(), tr("All Files (*)"));
    if (!path.isEmpty())
      m_documentManager->openFile(path);
  };

  cb.importFiles  = [this]() { onImportFilesDialog(); };
  cb.importFolder = [this]() { onImportFolderDialog(); };

  cb.save = [this]() {
    if (m_centralStack && m_centralStack->currentIndex() == 1) {
      if (m_overseerPage)
        m_overseerPage->saveAll();
    } else {
      m_documentManager->save();
    }
  };

  cb.saveAll = [this]() {
    if (m_centralStack && m_centralStack->currentIndex() == 1) {
      if (m_overseerPage)
        m_overseerPage->saveAll();
    } else {
      for (TextDocument *doc : m_documentManager->openDocuments()) {
        if (doc && doc->isModified())
          m_documentManager->saveDocument(doc);
      }
    }
  };

  cb.exit = [this]() { close(); };

  cb.modeNormal   = [this]() { setMode(Mode::Normal); };
  cb.modeOverseer = [this]() { setMode(Mode::Overseer); };
  cb.modeSearch   = [this]() { setMode(Mode::Search); };

  cb.toggleSpeech = [this]() { onToggleSpeechPanel(); };
  cb.talkToLore   = [this]() { enterAssistantShell(); };

  cb.openSettings    = [this]() { openSettings(); };
  cb.openLlmSettings = [this]() { openLlmSettings(); };
  cb.manageModels    = [this]() { manageModels(); };

  cb.rebuildIndex = [this]() {
    if (m_scopeIndex) {
      m_scopeIndex->rebuild(notesRootPath());
      m_searchIndexNeedsBuild = false;
    }
  };

  cb.selectNormalTheme   = [this](const QString &n) { applyNormalTheme(n); };
  cb.selectOverseerTheme = [this](const QString &n) { applyOverseerTheme(n); };

  cb.about   = [this]() { about(); };
  cb.aboutQt = [this]() { aboutQt(); };

  m_titleBar->setCallbacks(cb);

  m_titleBar->setNormalThemes(
      ThemeRegistry::instance().selectableNames(), m_currentNormalTheme);
  m_titleBar->setOverseerThemes(
      ThemeRegistry::instance().selectableNames(), m_currentOverseerTheme);
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

  if (m_searchPage) {
    m_searchPage->setStyleSheet(combined);
    m_searchPage->setPalette(paletteForTokens(tokens));
  }

  if (m_assistantShell) {
    m_assistantShell->setStyleSheet(combined);
    m_assistantShell->setPalette(paletteForTokens(tokens));
    m_assistantShell->setThemeTokens(tokens);
  }

  if (m_documentArea)
    m_documentArea->setThemeTokens(tokens);

  if (m_titleBar) {
    m_titleBar->setStyleSheet(combined);
    m_titleBar->setPalette(paletteForTokens(tokens));
  }

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

  if (m_titleBar) {
    m_titleBar->setStyleSheet(combined);
    m_titleBar->setPalette(paletteForTokens(tokens));
  }

  if (m_assistantShell) {
    m_assistantShell->setStyleSheet(combined);
    m_assistantShell->setPalette(paletteForTokens(tokens));
    m_assistantShell->setThemeTokens(tokens);
  }

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

  connect(m_documentManager, &DocumentManager::currentDocumentChanged,
          m_documentArea, [this](TextDocument *document) {
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

  m_normalCenterRow = new QWidget(m_normalPage);
  auto *rowLayout = new QHBoxLayout(m_normalCenterRow);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->setSpacing(0);

  m_fileTreeDock =
      new AutoHideDock(AutoHideDock::Edge::Left,
                       QStringLiteral("normal/fileTree"), m_normalCenterRow);

  m_fileTreeReservation = new DockReservation(m_fileTreeDock, m_normalCenterRow);

  m_fileWidget = new FileWidget(m_fileTreeDock);
  m_fileTreeDock->setContent(m_fileWidget);

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

  m_chatDock =
      new AutoHideDock(AutoHideDock::Edge::Right,
                       QStringLiteral("normal/chat"), m_normalCenterRow);
  m_chatReservation = new DockReservation(m_chatDock, m_normalCenterRow);
  m_chatDock->setContent(m_chatWidget);

  rowLayout->addWidget(m_fileTreeReservation, 0);
  rowLayout->addWidget(m_documentArea, 1);
  rowLayout->addWidget(m_chatReservation, 0);

  if (auto *view = m_fileWidget->view()) {
    connect(view, &FileSystemView::preferredContentWidthChanged,
            m_fileTreeDock, [this](int width) {
              m_fileTreeDock->setPreferredContentLength(width);
              m_fileTreeDock->fitToContentLength();
            });
  }

  m_chatDock->setPreferredContentLength(
      std::max(m_chatWidget->sizeHint().width(),
               m_chatWidget->minimumSizeHint().width()));

  auto *layout = new QVBoxLayout(m_normalPage);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_normalCenterRow);

  m_fileTreeDock->showDock();
  m_chatDock->showDock();

  QTimer::singleShot(0, this, [this]() {
    if (!m_fileWidget || !m_fileTreeDock)
      return;

    if (auto *view = m_fileWidget->view()) {
      const int measured = view->measuredContentWidth();
      if (measured > 0) {
        m_fileTreeDock->setPreferredContentLength(measured);
        m_fileTreeDock->fitToContentLength();
      }
    }

    if (m_chatDock && m_chatWidget) {
      const int target =
          std::max(m_chatWidget->sizeHint().width(),
                   m_chatWidget->minimumSizeHint().width());
      m_chatDock->setPreferredContentLength(target);
      m_chatDock->fitToContentLength();
    }
  });
}

void MainWindow::buildOverseerPage() {
  m_overseerSessionManager =
      new OverseerSessionManager(m_inferenceService, this);

  m_overseerPage =
      new OverseerPage(m_inferenceService, m_editSession,
                       m_overseerSessionManager, this);

  connect(m_overseerPage, &OverseerPage::statusMessage, this,
          [this](const QString &text, int timeoutMs) {
            statusBar()->showMessage(text, timeoutMs);
          });

  connect(m_overseerPage, &OverseerPage::openFileInNormalEditorRequested,
          this, [this](const QString &absolutePath) {
            if (m_documentManager)
              m_documentManager->openFile(absolutePath);

            setMode(Mode::Normal);
          });

  connect(m_overseerPage->fileWidget(),
          &FileWidget::promoteToNotesRequested, this,
          [this](const QStringList &paths) {
            if (!m_notePromoter || !m_overseerSessionManager) {
              return;
            }

            const QString session =
                m_overseerSessionManager->activeSessionName();

            if (session.isEmpty()) {
              NotificationService::instance().warning(
                  tr("Promote"),
                  tr("No session is open."));
              return;
            }

            const QString notesRoot = notesRootPath();

            int written = 0;
            int skipped = 0;
            int copiedNotIndexed = 0;

            for (const QString &path : paths) {
              const NotePromoter::Result result =
                  m_notePromoter->promote(path, session, notesRoot);

              written += result.written.size();
              skipped += result.skipped.size();
              copiedNotIndexed += result.copiedNotIndexed.size();
            }

            QString body = tr("%1 file(s) added to notes/%2, %3 skipped.")
                               .arg(written)
                               .arg(session)
                               .arg(skipped);

            if (copiedNotIndexed > 0) {
              body += tr(" %1 copied but not indexed.")
                          .arg(copiedNotIndexed);
            }

            NotificationService::instance().info(tr("Promoted"), body);
          });
}

void MainWindow::bindCurrentEditor(TextEdit *editor) {
  if (!editor)
    return;

  if (m_editSession)
    m_editSession->setEditor(editor);

  if (m_chatWidget)
    m_chatWidget->setActiveEditor(editor);

  if (m_loreTriggers.contains(editor)) {
    return;
  }

  auto *trigger = new LoreTrigger(editor, this);

  connect(trigger, &LoreTrigger::triggerDetected, this,
          [this, editor](int position) {
            const QPoint globalPos =
                editor->mapToGlobal(editor->cursorRect().bottomRight());

            LoreInputDialog dialog(globalPos, this);

            if (dialog.exec() != QDialog::Accepted) {
              return;
            }

            const QString query = dialog.request();

            if (query.isEmpty()) {
              return;
            }

            if (!m_searchService || !m_inferenceService) {
              return;
            }

            auto *loop = new RetrievalLoop(m_searchService,
                                           m_inferenceService, this);

            auto anchorStart = std::make_shared<int>(position);
            auto anchorLength = std::make_shared<int>(0);

            connect(loop, &RetrievalLoop::stageChanged, this,
                    [editor, anchorStart,
                     anchorLength](const QString &label) {
                      QTextDocument *document = editor->document();
                      if (!document) {
                        return;
                      }

                      QTextCursor cursor(document);

                      if (*anchorLength > 0) {
                        const int end = *anchorStart + *anchorLength;
                        cursor.setPosition(*anchorStart);
                        cursor.setPosition(end,
                                           QTextCursor::KeepAnchor);
                        cursor.removeSelectedText();
                      }

                      cursor.setPosition(*anchorStart);

                      const QString placeholder =
                          QStringLiteral("*%1*").arg(label);

                      cursor.insertText(placeholder);

                      *anchorLength = placeholder.length();
                    });

            auto accumulated = std::make_shared<QString>();

            connect(loop, &RetrievalLoop::answerChunk, this,
                    [editor, anchorStart, anchorLength,
                     accumulated](const QString &chunk) {
                      *accumulated += chunk;

                      QTextDocument *document = editor->document();
                      if (!document) {
                        return;
                      }

                      QTextCursor cursor(document);

                      if (*anchorLength > 0) {
                        const int end =
                            *anchorStart + *anchorLength;
                        cursor.setPosition(*anchorStart);
                        cursor.setPosition(end, QTextCursor::KeepAnchor);
                        cursor.removeSelectedText();
                      }

                      cursor.setPosition(*anchorStart);
                      cursor.insertText(*accumulated);

                      *anchorLength = accumulated->length();

                      QTextCursor visible(document);
                      visible.setPosition(*anchorStart + *anchorLength);
                      editor->setTextCursor(visible);
                    });

            connect(loop, &RetrievalLoop::finished, this,
                    [editor, anchorStart, anchorLength, loop](
                        const QString &answer) {
                      QTextDocument *document = editor->document();
                      if (document) {
                        QTextCursor cursor(document);

                        if (*anchorLength > 0) {
                          const int end =
                              *anchorStart + *anchorLength;
                          cursor.setPosition(*anchorStart);
                          cursor.setPosition(end,
                                             QTextCursor::KeepAnchor);
                          cursor.removeSelectedText();
                        }

                        cursor.setPosition(*anchorStart);
                        cursor.insertText(answer);

                        QTextCursor visible(document);
                        visible.setPosition(*anchorStart + answer.length());
                        editor->setTextCursor(visible);
                      }

                      *anchorLength = 0;

                      loop->deleteLater();
                    });

            connect(loop, &RetrievalLoop::failed, this,
                    [editor, anchorStart, anchorLength, loop](
                        const QString &reason) {
                      QTextDocument *document = editor->document();
                      if (document) {
                        QTextCursor cursor(document);

                        if (*anchorLength > 0) {
                          const int end =
                              *anchorStart + *anchorLength;
                          cursor.setPosition(*anchorStart);
                          cursor.setPosition(end,
                                             QTextCursor::KeepAnchor);
                          cursor.removeSelectedText();
                        }

                        cursor.setPosition(*anchorStart);
                        cursor.insertText(QStringLiteral("> ") + reason);

                        QTextCursor visible(document);
                        visible.setPosition(*anchorStart +
                                            reason.length() + 2);
                        editor->setTextCursor(visible);
                      }

                      *anchorLength = 0;

                      loop->deleteLater();
                    });

            loop->start(query);
          });

  m_loreTriggers.insert(editor, trigger);
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
  if (m_centralStack) {
    settings.setValue(ModeKey, m_centralStack->currentIndex());
  }

  event->accept();
}

void MainWindow::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
}

void MainWindow::moveEvent(QMoveEvent *event) {
  QMainWindow::moveEvent(event);
}

void MainWindow::changeEvent(QEvent *event) {
  QMainWindow::changeEvent(event);
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

void MainWindow::setMode(Mode mode) {
  if (!m_centralStack) {
    return;
  }

  const int targetIndex = static_cast<int>(mode);

  if (m_centralStack->currentIndex() != targetIndex) {
    if (targetIndex == static_cast<int>(Mode::Overseer) &&
        !confirmDiscardChanges(tr("Normal mode"))) {
      return;
    }

    if (targetIndex != static_cast<int>(Mode::Overseer) &&
        m_overseerPage && m_centralStack->currentIndex() == 1 &&
        !confirmDiscardChanges(tr("Overseer mode"))) {
      return;
    }

    m_centralStack->setCurrentIndex(targetIndex);
  }

  if (mode == Mode::Overseer) {
    ThemeRegistry::instance().setActiveTheme(m_currentOverseerTheme);
  } else {
    ThemeRegistry::instance().setActiveTheme(m_currentNormalTheme);
  }

  if (m_titleBar) {
    switch (mode) {
    case Mode::Overseer:
      m_titleBar->setModeText(tr("Overseer"));
      m_titleBar->setModeIcon(QIcon::fromTheme(QStringLiteral("view-grid")));
      break;
    case Mode::Search:
      m_titleBar->setModeText(tr("Search"));
      m_titleBar->setModeIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
      break;
    case Mode::Normal:
    default:
      m_titleBar->setModeText(tr("Normal"));
      m_titleBar->setModeIcon(
          QIcon::fromTheme(QStringLiteral("document-edit")));
      break;
    }

    m_titleBar->setModeChecked(targetIndex);
  }

  if (mode == Mode::Search && m_searchPage) {
    m_searchPage->focusQuery();
  }

  QSettings settings;
  settings.setValue(ModeKey, targetIndex);
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

QList<MainWindow::ImportCandidate> MainWindow::collectImportableFilesIn(
    const QString &folderPath) const {
  QList<ImportCandidate> result;

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

  const QDir root(folderPath);

  QDirIterator it(folderPath, patterns, QDir::Files | QDir::Readable,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    const QString absolute = it.next();

    ImportCandidate candidate;
    candidate.absolutePath = absolute;
    candidate.relativeSubpath = root.relativeFilePath(absolute);

    const int slash = candidate.relativeSubpath.lastIndexOf(QChar('/'));

    if (slash >= 0) {
      candidate.relativeSubpath = candidate.relativeSubpath.left(slash);
    } else {
      candidate.relativeSubpath.clear();
    }

    result.append(candidate);
  }

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

  const QList<ImportCandidate> candidates =
      collectImportableFilesIn(folder);

  if (candidates.isEmpty()) {
    reportImportSummary(0, 0, 0);
    return;
  }

  const QString folderName = QFileInfo(folder).fileName();

  m_importQueue.clear();

  for (const ImportCandidate &candidate : candidates) {
    ImportCandidate adjusted = candidate;
    adjusted.relativeSubpath =
        candidate.relativeSubpath.isEmpty()
            ? folderName
            : folderName + QLatin1Char('/') + candidate.relativeSubpath;
    m_importQueue.append(adjusted);
  }

  m_bulkImportSucceeded = 0;
  m_bulkImportFailed = 0;
  m_bulkImportTotal = m_importQueue.size();
  m_bulkImportCompleted = 0;
  m_bulkImportCancelled = false;
  m_importInFlight = 0;
  m_bulkImportTokens.clear();

  m_importProgress = new QProgressDialog(
      tr("Importing %1 file(s)…").arg(m_importQueue.size()), tr("Cancel"), 0,
      m_importQueue.size(), this);
  m_importProgress->setWindowTitle(tr("Import"));
  m_importProgress->setWindowModality(Qt::WindowModal);
  m_importProgress->setMinimumDuration(0);
  m_importProgress->setAutoClose(false);
  m_importProgress->setAutoReset(false);
  m_importProgress->show();

  connect(m_importProgress, &QProgressDialog::canceled, this,
          &MainWindow::onBulkImportCancelled);

  for (int i = 0; i < ImportConcurrency && !m_importQueue.isEmpty(); ++i) {
    startNextImport();
  }
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
  if (paths.isEmpty() || !m_ingestService) {
    return;
  }

  m_importQueue.clear();

  for (const QString &path : paths) {
    ImportCandidate candidate;
    candidate.absolutePath = path;
    candidate.relativeSubpath.clear();
    m_importQueue.append(candidate);
  }

  m_bulkImportSucceeded = 0;
  m_bulkImportFailed = 0;
  m_bulkImportTotal = m_importQueue.size();
  m_bulkImportCompleted = 0;
  m_bulkImportCancelled = false;
  m_importInFlight = 0;
  m_bulkImportTokens.clear();

  m_importProgress = new QProgressDialog(
      tr("Importing %1 file(s)…").arg(m_importQueue.size()), tr("Cancel"), 0,
      m_importQueue.size(), this);
  m_importProgress->setWindowTitle(tr("Import"));
  m_importProgress->setWindowModality(Qt::WindowModal);
  m_importProgress->setMinimumDuration(0);
  m_importProgress->setAutoClose(false);
  m_importProgress->setAutoReset(false);
  m_importProgress->show();

  connect(m_importProgress, &QProgressDialog::canceled, this,
          &MainWindow::onBulkImportCancelled);

  for (int i = 0; i < ImportConcurrency && !m_importQueue.isEmpty(); ++i) {
    startNextImport();
  }
}

void MainWindow::startNextImport() {
  if (!m_ingestService || m_importQueue.isEmpty() || m_bulkImportCancelled) {
    return;
  }

  const ImportCandidate candidate = m_importQueue.takeFirst();

  IngestOptions options;
  options.destinationFolder = notesRootPath();
  options.relativeSubpath = candidate.relativeSubpath;
  options.writeProvenance = true;
  options.sectionPerPage = true;

  ++m_importInFlight;

  const quint64 token = m_ingestService->import(
      candidate.absolutePath, options,
      [this, token](IngestService::Outcome outcome) {
        onBulkImportCompleted(token, outcome.ok());

        if (outcome.ok()) {
          if (m_scopeIndex) {
            m_scopeIndex->addFile(outcome.notePath);
          }
        } else if (!m_bulkImportCancelled) {
          reportImportFailure(QString(), outcome.error);
        }

        --m_importInFlight;

        if (!m_importQueue.isEmpty() && !m_bulkImportCancelled) {
          startNextImport();
        } else if (m_importQueue.isEmpty() && m_importInFlight == 0) {
          finishBulkImport();
        }
      });

  if (token == 0) {
    --m_importInFlight;

    if (!m_importQueue.isEmpty() && !m_bulkImportCancelled) {
      startNextImport();
    } else if (m_importQueue.isEmpty() && m_importInFlight == 0) {
      finishBulkImport();
    }
  } else {
    m_bulkImportTokens.append(token);
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
}

void MainWindow::onBulkImportCancelled() {
  if (m_bulkImportCancelled) {
    return;
  }
  m_bulkImportCancelled = true;

  m_importQueue.clear();

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
        if (m_scopeIndex) {
          m_scopeIndex->addFile(outcome.notePath);
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

void MainWindow::buildSearchLayer() {
  m_scopeIndex = std::make_unique<ScopeIndex>(m_inferenceService);

  const QString appData =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  const QString indexDir = appData + QStringLiteral("/search");

  m_scopeIndex->setIndexDirectory(indexDir);

  const QStringList additionalRoots = {
      appData + QStringLiteral("/assistant"),
  };

  m_scopeIndex->setAdditionalRoots(additionalRoots);

  const bool loaded = m_scopeIndex->load();

  m_searchIndexNeedsBuild = !loaded;

  if (!loaded) {
    qDebug() << "[MainWindow] Scheduling search index build.";
    QTimer::singleShot(0, this, [this]() {
      if (!m_scopeIndex) {
        return;
      }
      statusBar()->showMessage(tr("Building the search index…"));
      m_scopeIndex->rebuild(notesRootPath());
      m_searchIndexNeedsBuild = false;
    });
  }

  m_notePromoter = new NotePromoter(m_scopeIndex.get(), this);

  m_searchService =
      new SearchService(m_inferenceService, m_scopeIndex.get(), this);

  m_searchPage =
      new SearchPage(m_searchService, m_inferenceService, this);

  connect(m_searchPage, &SearchPage::openRequested, this,
          &MainWindow::onSearchOpenRequested);

  connect(m_scopeIndex.get(), &ScopeIndex::progress, this,
          [this](int current, int total) {
            statusBar()->showMessage(
                tr("Indexing %1 / %2").arg(current).arg(total));
          });

  connect(m_scopeIndex.get(), &ScopeIndex::finished, this,
          [this](int scopes) {
            if (scopes < 0) {
              statusBar()->showMessage(tr("Index build failed."), 5000);
            } else {
              statusBar()->showMessage(
                  tr("Index built: %1 scopes.").arg(scopes), 5000);
              m_searchIndexNeedsBuild = false;
            }
          });
}

void MainWindow::onSearchRequested() {
  if (m_searchIndexNeedsBuild && m_scopeIndex) {
    statusBar()->showMessage(tr("Building the search index…"));
    m_scopeIndex->rebuild(notesRootPath());
    m_searchIndexNeedsBuild = false;
  }

  setMode(Mode::Search);
}

void MainWindow::onSearchOpenRequested(const QString &filePath,
                                       const QString &scopeId) {
  Q_UNUSED(scopeId);

  if (filePath.isEmpty() || !m_documentManager) {
    return;
  }

  m_documentManager->openFile(filePath);

  setMode(Mode::Normal);
}

void MainWindow::onAssistantMessageSubmitted(const QString &text) {
  if (!m_assistant || !m_assistantShell) {
    return;
  }

  m_assistantShell->setBusy(true);
  m_assistant->handleUserMessage(text);
}
void MainWindow::onDocumentSaved(TextDocument *document) {
  if (!document || !m_scopeIndex) {
    return;
  }

  const QString path = document->filePath();

  if (path.isEmpty()) {
    return;
  }

  const QString notesRoot = notesRootPath();

  if (!path.startsWith(notesRoot)) {
    return;
  }

  m_scopeIndex->markDirty(path);
}