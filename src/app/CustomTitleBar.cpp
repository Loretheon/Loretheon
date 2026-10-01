#include "CustomTitleBar.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QScreen>
#include <QStyle>
#include <QToolButton>
#include <QWindow>

namespace {
constexpr int kTitleBarHeight = 32;
constexpr int kButtonSize = 32;
constexpr int kSnapThreshold = 5;
constexpr int kModeButtonMinWidth = 104;
constexpr int kLoreButtonMinWidth = 72;
} // namespace

CustomTitleBar::CustomTitleBar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("customTitleBar"));
  setMinimumHeight(kTitleBarHeight);
  setMaximumHeight(kTitleBarHeight);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 0, 0, 0);
  layout->setSpacing(0);

  m_menuBar = new QMenuBar(this);
  m_menuBar->setObjectName(QStringLiteral("titleBarMenuBar"));
  m_menuBar->setNativeMenuBar(false);
  m_menuBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  m_menuBar->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_menuBar);

  buildMenus();
  buildToolbarArea();

  layout->addStretch(1);

  layout->addWidget(m_modeButton);
  layout->addSpacing(6);
  layout->addWidget(m_loreButton);
  layout->addSpacing(12);

  const auto makeWindowButton = [this](QStyle::StandardPixmap pixmap,
                                       const QString &name) {
    auto *btn = new QToolButton(this);
    btn->setObjectName(name);
    btn->setProperty("windowControl", true);
    btn->setIcon(QApplication::style()->standardIcon(pixmap));
    btn->setFixedSize(kButtonSize, kButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setAutoRaise(true);
    btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    return btn;
  };

  m_minimizeBtn = makeWindowButton(QStyle::SP_TitleBarMinButton,
                                   QStringLiteral("titleBarMinimize"));
  m_maximizeBtn = makeWindowButton(QStyle::SP_TitleBarMaxButton,
                                   QStringLiteral("titleBarMaximize"));
  m_closeBtn = makeWindowButton(QStyle::SP_TitleBarCloseButton,
                                QStringLiteral("titleBarClose"));
  m_closeBtn->setProperty("windowControlDestructive", true);

  layout->addWidget(m_minimizeBtn);
  layout->addWidget(m_maximizeBtn);
  layout->addWidget(m_closeBtn);

  connect(m_minimizeBtn, &QToolButton::clicked, this,
          &CustomTitleBar::minimizeRequested);
  connect(m_maximizeBtn, &QToolButton::clicked, this,
          &CustomTitleBar::maximizeRequested);
  connect(m_closeBtn, &QToolButton::clicked, this,
          &CustomTitleBar::closeRequested);

  if (QWidget *win = window()) {
    win->installEventFilter(this);
  }
}

void CustomTitleBar::setCallbacks(const Callbacks &callbacks) {
  m_callbacks = callbacks;
}

void CustomTitleBar::buildMenus() {
  // ---- File ----
  m_fileMenu = m_menuBar->addMenu(tr("&File"));

  auto *newMenu = m_fileMenu->addMenu(tr("&New"));

  auto *newTextAct = newMenu->addAction(tr("&Text File"));
  newTextAct->setShortcuts(QKeySequence::New);
  connect(newTextAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.newText) m_callbacks.newText(); });

  auto *newMarkdownAct = newMenu->addAction(tr("&Markdown File"));
  connect(newMarkdownAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.newMarkdown) m_callbacks.newMarkdown(); });

  auto *newPlantUmlAct = newMenu->addAction(tr("&PlantUML Diagram"));
  connect(newPlantUmlAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.newPlantUml) m_callbacks.newPlantUml(); });

  auto *newDotAct = newMenu->addAction(tr("&Graphviz Diagram"));
  connect(newDotAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.newDot) m_callbacks.newDot(); });

  auto *newMermaidAct = newMenu->addAction(tr("&Mermaid Diagram"));
  connect(newMermaidAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.newMermaid) m_callbacks.newMermaid(); });

  auto *newHtmlAct = newMenu->addAction(tr("&HTML File"));
  connect(newHtmlAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.newHtml) m_callbacks.newHtml(); });

  auto *openAct = m_fileMenu->addAction(tr("&Open..."));
  openAct->setShortcuts(QKeySequence::Open);
  connect(openAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.open) m_callbacks.open(); });

  m_fileMenu->addSeparator();

  auto *importFilesAct = m_fileMenu->addAction(tr("Import &Files..."));
  connect(importFilesAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.importFiles) m_callbacks.importFiles(); });

  auto *importFolderAct = m_fileMenu->addAction(tr("Import F&older..."));
  connect(importFolderAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.importFolder) m_callbacks.importFolder(); });

  m_fileMenu->addSeparator();

  auto *saveAct = m_fileMenu->addAction(tr("&Save"));
  saveAct->setShortcuts(QKeySequence::Save);
  connect(saveAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.save) m_callbacks.save(); });

  auto *saveAllAct = m_fileMenu->addAction(tr("Save A&ll"));
  saveAllAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
  connect(saveAllAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.saveAll) m_callbacks.saveAll(); });

  m_fileMenu->addSeparator();

  auto *exitAct = m_fileMenu->addAction(tr("E&xit"));
  exitAct->setShortcuts(QKeySequence::Quit);
  connect(exitAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.exit) m_callbacks.exit(); });

  // ---- View ----
  m_viewMenu = m_menuBar->addMenu(tr("&View"));

  auto *modeGroup = new QActionGroup(this);
  modeGroup->setExclusive(true);

  m_normalModeAct = new QAction(tr("Normal"), this);
  m_normalModeAct->setCheckable(true);
  m_normalModeAct->setChecked(true);
  m_normalModeAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
  modeGroup->addAction(m_normalModeAct);
  connect(m_normalModeAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.modeNormal) m_callbacks.modeNormal(); });

  m_overseerModeAct = new QAction(tr("Overseer"), this);
  m_overseerModeAct->setCheckable(true);
  m_overseerModeAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
  modeGroup->addAction(m_overseerModeAct);
  connect(m_overseerModeAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.modeOverseer) m_callbacks.modeOverseer(); });

  m_searchModeAct = new QAction(tr("Search"), this);
  m_searchModeAct->setCheckable(true);
  m_searchModeAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
  modeGroup->addAction(m_searchModeAct);
  connect(m_searchModeAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.modeSearch) m_callbacks.modeSearch(); });

  auto *toggleSpeechAct = m_viewMenu->addAction(tr("Voice Panel"));
  toggleSpeechAct->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space));
  connect(toggleSpeechAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.toggleSpeech) m_callbacks.toggleSpeech(); });

  // ---- Tools ----
  m_toolsMenu = m_menuBar->addMenu(tr("&Tools"));

  auto *settingsAct = m_toolsMenu->addAction(tr("&Settings..."));
  connect(settingsAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.openSettings) m_callbacks.openSettings(); });

  m_toolsMenu->addSeparator();

  auto *llmAct = m_toolsMenu->addAction(tr("LLM &Settings..."));
  connect(llmAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.openLlmSettings) m_callbacks.openLlmSettings(); });

  m_toolsMenu->addSeparator();

  auto *modelsAct = m_toolsMenu->addAction(tr("&Manage Models..."));
  connect(modelsAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.manageModels) m_callbacks.manageModels(); });

  m_toolsMenu->addSeparator();

  auto *rebuildAct = m_toolsMenu->addAction(tr("Rebuild Search Index"));
  connect(rebuildAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.rebuildIndex) m_callbacks.rebuildIndex(); });

  // ---- Theme ----
  m_themeMenu = m_menuBar->addMenu(tr("&Theme"));
  m_normalThemeMenu = m_themeMenu->addMenu(tr("Normal"));
  m_overseerThemeMenu = m_themeMenu->addMenu(tr("Overseer"));

  // ---- Help ----
  m_helpMenu = m_menuBar->addMenu(tr("&Help"));

  auto *aboutAct = m_helpMenu->addAction(tr("&About"));
  connect(aboutAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.about) m_callbacks.about(); });

  auto *aboutQtAct = m_helpMenu->addAction(tr("About &Qt"));
  connect(aboutQtAct, &QAction::triggered, this,
          [this]() { if (m_callbacks.aboutQt) m_callbacks.aboutQt(); });
}

void CustomTitleBar::buildToolbarArea() {
  m_modeButton = new QToolButton(this);
  m_modeButton->setObjectName(QStringLiteral("titleBarMode"));
  m_modeButton->setPopupMode(QToolButton::InstantPopup);
  m_modeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_modeButton->setText(tr("Normal"));
  m_modeButton->setFocusPolicy(Qt::NoFocus);
  m_modeButton->setMinimumWidth(kModeButtonMinWidth);
  m_modeButton->setMinimumHeight(kTitleBarHeight - 6);
  m_modeButton->setAutoRaise(true);

  m_modeMenu = new QMenu(m_modeButton);
  m_modeMenu->addAction(m_normalModeAct);
  m_modeMenu->addAction(m_overseerModeAct);
  m_modeMenu->addAction(m_searchModeAct);
  m_modeButton->setMenu(m_modeMenu);

  m_loreButton = new QToolButton(this);
  m_loreButton->setObjectName(QStringLiteral("titleBarLore"));
  m_loreButton->setText(tr("Lore"));
  m_loreButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_loreButton->setAutoRaise(true);
  m_loreButton->setFocusPolicy(Qt::NoFocus);
  m_loreButton->setMinimumSize(kLoreButtonMinWidth, kTitleBarHeight - 6);
  m_loreButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  connect(m_loreButton, &QToolButton::clicked, this,
          [this]() { if (m_callbacks.talkToLore) m_callbacks.talkToLore(); });
}

void CustomTitleBar::setNormalThemes(const QStringList &names,
                                     const QString &current) {
  m_normalThemeMenu->clear();

  auto *group = new QActionGroup(m_normalThemeMenu);
  group->setExclusive(true);

  for (const QString &name : names) {
    auto *a = m_normalThemeMenu->addAction(name);
    a->setCheckable(true);
    a->setChecked(name == current);
    group->addAction(a);

    connect(a, &QAction::triggered, this, [this, name]() {
      if (m_callbacks.selectNormalTheme) m_callbacks.selectNormalTheme(name);
    });
  }
}

void CustomTitleBar::setOverseerThemes(const QStringList &names,
                                       const QString &current) {
  m_overseerThemeMenu->clear();

  auto *group = new QActionGroup(m_overseerThemeMenu);
  group->setExclusive(true);

  for (const QString &name : names) {
    auto *a = m_overseerThemeMenu->addAction(name);
    a->setCheckable(true);
    a->setChecked(name == current);
    group->addAction(a);

    connect(a, &QAction::triggered, this, [this, name]() {
      if (m_callbacks.selectOverseerTheme) m_callbacks.selectOverseerTheme(name);
    });
  }
}

void CustomTitleBar::setModeText(const QString &text) {
  m_modeButton->setText(text);
}

void CustomTitleBar::setModeIcon(const QIcon &icon) {
  m_modeButton->setIcon(icon);
}

void CustomTitleBar::setModeChecked(int modeIndex) {
  switch (modeIndex) {
  case 1:
    if (m_overseerModeAct) m_overseerModeAct->setChecked(true);
    break;
  case 2:
    if (m_searchModeAct) m_searchModeAct->setChecked(true);
    break;
  default:
    if (m_normalModeAct) m_normalModeAct->setChecked(true);
    break;
  }
}

void CustomTitleBar::toggleMaximizeRestore() {
  QWidget *win = window();
  if (win->isMaximized()) {
    win->showNormal();
  } else {
    win->showMaximized();
  }
}

void CustomTitleBar::updateMaximizeIcon() {
  QWidget *win = window();
  const bool maxed = win && win->isMaximized();

  m_maximizeBtn->setIcon(QApplication::style()->standardIcon(
      maxed ? QStyle::SP_TitleBarNormalButton
            : QStyle::SP_TitleBarMaxButton));
}

bool CustomTitleBar::eventFilter(QObject *watched, QEvent *event) {
  if (watched == window() && event->type() == QEvent::WindowStateChange) {
    updateMaximizeIcon();
  }
  return QWidget::eventFilter(watched, event);
}

bool CustomTitleBar::isDragRegion(const QPoint &pos) const {
  for (QWidget *child :
       findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
    if (child->isVisible() && child->geometry().contains(pos))
      return false;
  }
  return true;
}

void CustomTitleBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton &&
      isDragRegion(event->position().toPoint())) {
    m_dragging = true;
    m_dragOrigin = event->globalPosition().toPoint();
    event->accept();
    return;
  }

  QWidget::mousePressEvent(event);
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent *event) {
  if (!m_dragging) {
    QWidget::mouseMoveEvent(event);
    return;
  }

  QWidget *win = window();
  const QPoint globalPos = event->globalPosition().toPoint();

  QScreen *screen = QGuiApplication::screenAt(globalPos);
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }

  const QRect screenGeom = screen->availableGeometry();

  if (globalPos.y() - screenGeom.top() < kSnapThreshold &&
      !win->isMaximized()) {
    win->showMaximized();
    m_dragging = false;
    event->accept();
    return;
  }

  if (QWindow *handle = win->windowHandle()) {
    if (handle->startSystemMove()) {
      m_dragging = false;
      event->accept();
      return;
    }
  }

  QWidget::mouseMoveEvent(event);
}

void CustomTitleBar::mouseReleaseEvent(QMouseEvent *event) {
  m_dragging = false;
  QWidget::mouseReleaseEvent(event);
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton &&
      isDragRegion(event->position().toPoint())) {
    toggleMaximizeRestore();
    event->accept();
    return;
  }

  QWidget::mouseDoubleClickEvent(event);
}