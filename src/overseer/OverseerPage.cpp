#include "../../include/overseer/OverseerPage.h"

#include "../../include/overseer/DockReservation.h"
#include "AutoHideDock.h"
#include "ConductorBoard.h"
#include "ConductorDock.h"
#include "OverseerSession.h"
#include "OverseerSessionList.h"
#include "OverseerSidePanel.h"
#include "OverseerStorage.h"
#include "OverseerWidget.h"
#include "OverviewPanel.h"
#include "PathUtils.h"
#include "TextEdit.h"
#include "ToastStack.h"
#include "TranscriptPanel.h"
#include "Workstation.h"
#include "WorkstationBar.h"
#include "WorkstationWindow.h"

#include "DocumentArea.h"
#include "DocumentManager.h"
#include "FileWidget.h"
#include "OverseerSessionManager.h"
#include "Settings.h"
#include "TextDocument.h"
#include "TextWidget.h"

#include "../../include/ai/edit/EditSession.h"

#include "inference/InferenceService.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr auto LegacyLayoutFilename = "workstation.json";
constexpr auto SessionsDirname = "Sessions";

} // namespace

OverseerPage::OverseerPage(InferenceService *inferenceService,
                           EditSession *editSession,
                           OverseerSessionManager *manager,
                           QWidget *parent)
    : QWidget(parent),
      m_inferenceService(inferenceService),
      m_editSession(editSession),
      m_manager(manager) {
  m_documentManager = new DocumentManager(this);

  m_overseer = new OverseerWidget(manager, this);
  m_workstation = new Workstation(m_documentManager, editSession, this);

  m_overseer->setWorkstation(m_workstation);

  if (m_manager)
    m_manager->setWorkstation(m_workstation);

  m_documentArea = new DocumentArea(m_documentManager, this);
  m_documentArea->setEditSession(editSession);
  m_documentArea->hide();

  m_fileWidget = new FileWidget(this);

  // The conductor dock. It lives on top of the page, not inside any
  // layout, and slides down from the top when triggered.
  m_conductorDock = new ConductorDock(this);
  m_conductorDock->setQueue(m_overseer->queue());
  m_conductorDock->setRoster(m_overseer->roster());
  m_conductorDock->setDependencies(m_overseer->dependencies());

  // The board's Remove button on failed cards routes back into the
  // conductor, which prompts the user about dependents and then
  // updates both the queue and the dependency graph.
  connect(m_conductorDock->board(), &ConductorBoard::removeRequested,
          m_overseer, &OverseerWidget::removeFailedRequest);

  m_dockTrigger = new QToolButton(this);
  m_dockTrigger->setObjectName(QStringLiteral("conductorDockTrigger"));
  m_dockTrigger->setText(tr("Conductor"));
  m_dockTrigger->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_dockTrigger->setAutoRaise(true);

  connect(m_dockTrigger, &QToolButton::clicked, m_conductorDock,
          &ConductorDock::toggle);

  connect(m_conductorDock, &ConductorDock::opened, this, [this]() {
    if (m_dockTrigger)
      m_dockTrigger->setText(tr("Conductor ▲"));
  });

  connect(m_conductorDock, &ConductorDock::closed, this, [this]() {
    if (m_dockTrigger)
      m_dockTrigger->setText(tr("Conductor"));
  });

  connect(m_overseer->sessionListPanel(),
          &OverseerSessionList::sessionSelected, this,
          &OverseerPage::onSessionSelected);

  connect(m_overseer->sessionListPanel(),
          &OverseerSessionList::sessionCleared, this,
          &OverseerPage::onSessionCleared);

  connect(m_fileWidget, &FileWidget::fileSelected, this,
          &OverseerPage::onFileSelected);

  connect(m_fileWidget, &FileWidget::addToOverseerRequested, this,
          &OverseerPage::onAddToOverview);

  connect(m_fileWidget, &FileWidget::newNoteRequested, m_documentManager,
          &DocumentManager::newMarkdownFileIn);

  connect(m_fileWidget, &FileWidget::newFolderRequested, m_documentManager,
          &DocumentManager::newFolderIn);

  connect(m_fileWidget, &FileWidget::deleteRequested, m_documentManager,
          &DocumentManager::deleteFile);

  connect(m_fileWidget, &FileWidget::renameRequested, m_documentManager,
          &DocumentManager::renameFile);

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

  connect(m_documentManager, &DocumentManager::documentChanged, this,
          [this](TextDocument *) { onDocumentChanged(); });

  connect(m_overseer, &OverseerWidget::fileWritten, this,
          [this](const QString &absolutePath) {
            if (!m_workstation)
              return;

            if (m_currentOutputFolder.isEmpty())
              return;

            if (!PathUtils::isUnder(absolutePath, m_currentOutputFolder))
              return;

            if (m_fileWidget)
              m_fileWidget->setRootPath(m_currentOutputFolder);

            if (WorkstationWindow *existing =
                    m_workstation->windowForPath(absolutePath)) {
              m_workstation->focusWindow(existing);
              return;
            }

            m_workstation->openFile(absolutePath);
          });

  connect(m_overseer, &OverseerWidget::fileOpenRequested, this,
          [this](const QString &absolutePath) {
            if (!m_workstation)
              return;

            if (m_currentOutputFolder.isEmpty())
              return;

            if (!PathUtils::isUnder(absolutePath, m_currentOutputFolder))
              return;

            m_workstation->openFile(absolutePath);
          });

  connect(m_overseer, &OverseerWidget::fileCloseRequested, this,
          [this](const QString &absolutePath) {
            if (!m_workstation)
              return;

            m_workstation->closeFile(absolutePath);
          });

  connect(m_overseer, &OverseerWidget::saveWorkstationFileRequested, this,
          [this](const QString &absolutePath) {
            if (!m_workstation || absolutePath.isEmpty())
              return;

            WorkstationWindow *window =
                m_workstation->windowForPath(absolutePath);

            if (window)
              m_workstation->saveWindowToDisk(window);
          });

  connect(m_overseer, &OverseerWidget::planGenerationStarted, this,
          [this](const QString &absolutePath) {
            if (!m_workstation)
              return;

            for (WorkstationWindow *w : m_workstation->windows()) {
              if (w && w->filePath() == absolutePath) {
                w->setStatus(WorkstationWindow::Status::Rewriting);
                return;
              }
            }
          });

  connect(m_overseer, &OverseerWidget::planReviewReady, this,
          [this](const QString &absolutePath) {
            if (!m_workstation)
              return;

            for (WorkstationWindow *w : m_workstation->windows()) {
              if (w && w->filePath() == absolutePath) {
                w->setStatus(WorkstationWindow::Status::Review);
                return;
              }
            }
          });

  connect(m_overseer, &OverseerWidget::planApplied, this,
          [this](const QString &absolutePath) {
            if (!m_workstation)
              return;

            for (WorkstationWindow *w : m_workstation->windows()) {
              if (w && w->filePath() == absolutePath) {
                w->setTransientStatus(WorkstationWindow::Status::Applied,
                                      tr("Applied"), 2500);
                return;
              }
            }
          });

  connect(m_overseer, &OverseerWidget::planFailed, this,
          [this](const QString &absolutePath) {
            if (!m_workstation || absolutePath.isEmpty())
              return;

            for (WorkstationWindow *w : m_workstation->windows()) {
              if (w && w->filePath() == absolutePath) {
                w->setTransientStatus(WorkstationWindow::Status::Failed,
                                      tr("Failed"), 3500);
                return;
              }
            }
          });

  connect(m_workstation, &Workstation::currentFileChanged, this,
          &OverseerPage::onFocusedFileChanged);

  connect(m_workstation, &Workstation::rewriteRequested, this,
          [this](WorkstationWindow *window) {
            Q_UNUSED(window);

            emit statusMessage(
                tr("Scoped rewrite is not yet wired to the Workstation."),
                4000);
          });

  if (auto *op = m_overseer->sidePanel()->overviewPanel()) {
    connect(op, &OverviewPanel::openRequested, this,
            [this](const QString &relativePath) {
              if (m_currentOutputFolder.isEmpty())
                return;

              const QString abs =
                  PathUtils::toAbsolute(relativePath, m_currentOutputFolder);

              if (!QFileInfo::exists(abs))
                return;

              if (WorkstationWindow *existing =
                      m_workstation->windowForPath(abs)) {
                m_workstation->focusWindow(existing);
                return;
              }

              m_workstation->openFile(abs);
            });

    connect(op, &OverviewPanel::openInNormalEditorRequested, this,
            [this](const QString &relativePath) {
              const QString notesRoot = Settings::getRootDirectory();
              const QString abs = PathUtils::toAbsolute(relativePath, notesRoot);

              if (QFileInfo::exists(abs))
                emit openFileInNormalEditorRequested(abs);
            });

    connect(op, &OverviewPanel::stageRequested, this,
            [this](const QString &relativePath) {
              const QString notesRoot = Settings::getRootDirectory();
              const QString abs = PathUtils::toAbsolute(relativePath, notesRoot);
              stageFileInSession(abs);
            });

    connect(op, &OverviewPanel::addFromTreeRequested, this, [this]() {
      emit statusMessage(tr("Drag a file from the tree to add a reference."),
                         4000);
    });
  }

  auto *leftContent = new QWidget(this);
  auto *leftLayout = new QVBoxLayout(leftContent);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(0);

  auto *leftSplitter = new QSplitter(Qt::Vertical, leftContent);
  leftSplitter->addWidget(m_overseer->sessionListPanel());
  leftSplitter->addWidget(m_fileWidget);

  leftLayout->addWidget(leftSplitter);

  m_leftDock = new AutoHideDock(AutoHideDock::Edge::Left, this);
  m_leftDock->setContent(leftContent);

  auto *rightContent = new QWidget(this);
  auto *rightLayout = new QVBoxLayout(rightContent);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(0);

  rightLayout->addWidget(m_overseer->sidePanel());

  m_rightDock = new AutoHideDock(AutoHideDock::Edge::Right, this);
  m_rightDock->setContent(rightContent);

  auto *workstationColumn = new QWidget(this);
  auto *workstationLayout = new QVBoxLayout(workstationColumn);
  workstationLayout->setContentsMargins(0, 0, 0, 0);
  workstationLayout->setSpacing(0);

  auto *workstationBar = new WorkstationBar(m_workstation, workstationColumn);
  workstationLayout->addWidget(workstationBar);
  workstationLayout->addWidget(m_workstation, 1);

  auto *centerColumn = new QSplitter(Qt::Vertical, this);
  centerColumn->addWidget(workstationColumn);

  auto *transcriptWrapper = new QWidget(this);
  auto *transcriptLayout = new QVBoxLayout(transcriptWrapper);
  transcriptLayout->setContentsMargins(0, 0, 0, 0);
  transcriptLayout->setSpacing(0);
  transcriptLayout->addWidget(m_overseer->transcriptPanel(), 1);
  transcriptLayout->addWidget(m_overseer, 0);

  centerColumn->addWidget(transcriptWrapper);
  centerColumn->setStretchFactor(0, 3);
  centerColumn->setStretchFactor(1, 1);

  // The trigger sits above the center column, in its own fixed-height
  // row. The conductor dock overlays the whole page from the top.
  auto *centerHost = new QWidget(this);
  auto *centerHostLayout = new QVBoxLayout(centerHost);
  centerHostLayout->setContentsMargins(0, 0, 0, 0);
  centerHostLayout->setSpacing(0);

  auto *triggerRow = new QWidget(centerHost);
  triggerRow->setObjectName(QStringLiteral("conductorTriggerRow"));
  auto *triggerLayout = new QHBoxLayout(triggerRow);
  triggerLayout->setContentsMargins(6, 2, 6, 2);
  triggerLayout->setSpacing(6);
  triggerLayout->addWidget(m_dockTrigger);
  triggerLayout->addStretch(1);

  centerHostLayout->addWidget(triggerRow);
  centerHostLayout->addWidget(centerColumn, 1);

  auto *leftReservation = new DockReservation(m_leftDock, this);
  auto *rightReservation = new DockReservation(m_rightDock, this);

  auto *columns = new QWidget(this);
  auto *columnsLayout = new QHBoxLayout(columns);
  columnsLayout->setContentsMargins(0, 0, 0, 0);
  columnsLayout->setSpacing(0);

  columnsLayout->addWidget(leftReservation, 0);
  columnsLayout->addWidget(centerHost, 1);
  columnsLayout->addWidget(rightReservation, 0);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->addWidget(columns);


  migrateLegacyLayoutFiles();
}

OverseerPage::~OverseerPage() = default;

void OverseerPage::migrateLegacyLayoutFiles() {
  const QString root =
      QDir(OverseerStorage::rootPath()).filePath(SessionsDirname);

  QDir sessionsDir(root);

  if (!sessionsDir.exists())
    return;

  const QStringList sessionNames =
      sessionsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

  for (const QString &name : sessionNames) {
    const QString sessionFolder = sessionsDir.filePath(name);

    const QString legacyPath =
        QDir(sessionFolder)
            .filePath(QStringLiteral("output/%1").arg(LegacyLayoutFilename));

    if (!QFileInfo::exists(legacyPath))
      continue;

    const QString newPath =
        QDir(sessionFolder).filePath(LegacyLayoutFilename);

    if (QFileInfo::exists(newPath)) {
      QFile::remove(legacyPath);
      continue;
    }

    QFile::rename(legacyPath, newPath);
  }
}

void OverseerPage::setThemeTokens(const ThemeTokens &tokens) {
  if (m_documentArea)
    m_documentArea->setThemeTokens(tokens);

  if (m_workstation)
    m_workstation->setThemeTokens(tokens);
}

bool OverseerPage::hasUnsavedChanges() const {
  if (!m_documentManager)
    return false;

  for (TextDocument *doc : m_documentManager->openDocuments()) {
    if (doc && doc->isModified())
      return true;
  }

  return false;
}

bool OverseerPage::saveAll() {
  if (!m_documentManager)
    return true;

  for (TextDocument *doc : m_documentManager->openDocuments()) {
    if (!doc || !doc->isModified())
      continue;

    if (!m_documentManager->saveDocument(doc))
      return false;
  }

  return true;
}

void OverseerPage::discardAll() {
  if (!m_documentManager)
    return;

  for (TextDocument *doc : m_documentManager->openDocuments()) {
    if (doc)
      doc->setModified(false);
  }
}

void OverseerPage::onSessionSelected(const QString &name) {
  reloadSession(name);
}

void OverseerPage::onSessionCleared() {
  m_currentSessionName.clear();
  m_currentOutputFolder.clear();

  if (m_conductorDock)
    m_conductorDock->setSessionFolder(QString());

  if (m_workstation)
    m_workstation->closeAll();

  closeAllSessionDocuments();

  emit dirtyChanged(false);
}

void OverseerPage::reloadSession(const QString &name) {
  if (name.isEmpty()) {
    onSessionCleared();
    return;
  }

  OverseerSession *session =
      OverseerSession::open(OverseerStorage::rootPath(), name, this);

  if (!session) {
    emit statusMessage(tr("Could not open session '%1'.").arg(name), 4000);
    return;
  }

  if (m_workstation)
    m_workstation->closeAll();

  closeAllSessionDocuments();

  m_currentSessionName = name;
  m_currentOutputFolder = session->outputPath();

  // Pass the session folder to the conductor board so it can persist
  // the splitter geometry between the dependency graph and the kanban.
  if (m_conductorDock)
    m_conductorDock->setSessionFolder(session->folderPath());

  if (m_workstation) {
    m_workstation->setOutputFolder(m_currentOutputFolder);
    m_workstation->loadLayout();
  }

  if (m_fileWidget) {
    m_fileWidget->setRootPath(m_currentOutputFolder);
  }

  session->deleteLater();

  emit statusMessage(tr("Session: %1").arg(name), 3000);
  emit dirtyChanged(hasUnsavedChanges());
}

void OverseerPage::closeAllSessionDocuments() {
  if (!m_documentManager)
    return;

  const QList<TextDocument *> docs = m_documentManager->openDocuments();

  for (TextDocument *doc : docs) {
    if (doc)
      m_documentManager->closeDocument(doc);
  }
}

void OverseerPage::onFileSelected(const QString &path) {
  if (!m_workstation || path.isEmpty())
    return;

  if (!m_currentOutputFolder.isEmpty() &&
      !PathUtils::isUnder(path, m_currentOutputFolder)) {
    return;
  }

  if (WorkstationWindow *existing = m_workstation->windowForPath(path)) {
    m_workstation->focusWindow(existing);
    return;
  }

  m_workstation->openFile(path);
}

void OverseerPage::onDocumentChanged() {
  emit dirtyChanged(hasUnsavedChanges());
}

void OverseerPage::onAddToOverview(const QStringList &paths) {
  if (m_overseer)
    m_overseer->addOverviewReferences(paths);
}

void OverseerPage::stageFileInSession(const QString &absolutePath) {
  if (absolutePath.isEmpty() || m_currentOutputFolder.isEmpty())
    return;

  const QFileInfo info(absolutePath);
  if (!info.exists() || !info.isFile())
    return;

  const QString dest =
      QDir(m_currentOutputFolder).filePath(info.fileName());

  if (QFileInfo::exists(dest) && dest != absolutePath) {
    const auto reply = QMessageBox::question(
        this, tr("Copy to session"),
        tr("A file named '%1' already exists in the session. Overwrite?")
            .arg(info.fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
      return;

    QFile::remove(dest);
  }

  if (!QFile::copy(absolutePath, dest)) {
    emit statusMessage(tr("Could not copy %1 into the session.")
                           .arg(info.fileName()), 4000);
    return;
  }

  if (m_fileWidget)
    m_fileWidget->setRootPath(m_currentOutputFolder);

  if (m_workstation) {
    if (WorkstationWindow *existing = m_workstation->windowForPath(dest)) {
      m_workstation->focusWindow(existing);
    } else {
      m_workstation->openFile(dest);
    }
  }

  emit statusMessage(tr("Staged %1 in the session.").arg(info.fileName()),
                     3000);
}

void OverseerPage::onFocusedFileChanged(const QString &absolutePath) {
  if (!m_overseer)
    return;

  m_overseer->setFocusedFilePath(absolutePath);

  if (absolutePath.isEmpty()) {
    m_overseer->setFocusedDocument(nullptr, nullptr);
    return;
  }

  TextDocument *document = nullptr;

  for (TextDocument *candidate : m_documentManager->openDocuments()) {
    if (candidate->filePath() == absolutePath) {
      document = candidate;
      break;
    }
  }

  TextEdit *editor = nullptr;

  if (m_workstation) {
    WorkstationWindow *window = m_workstation->focusedWindow();

    if (window) {
      if (auto *textWidget = qobject_cast<TextWidget *>(window->body())) {
        editor = textWidget->editor();
      }
    }
  }

  m_overseer->setFocusedDocument(document, editor);
}

void OverseerPage::setWorkstationOnManager() {
  if (m_manager && m_workstation)
    m_manager->setWorkstation(m_workstation);
}