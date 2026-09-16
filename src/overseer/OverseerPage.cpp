#include "../../include/overseer/OverseerPage.h"

#include "../../include/overseer/OverseerSession.h"
#include "../../include/overseer/OverseerSessionList.h"
#include "../../include/overseer/OverseerSidePanel.h"
#include "../../include/overseer/OverseerStorage.h"
#include "../../include/overseer/OverseerWidget.h"
#include "../../include/overseer/OverviewPanel.h"
#include "../../include/overseer/PathUtils.h"
#include "../../include/overseer/TranscriptPanel.h"
#include "../../include/overseer/Workstation.h"
#include "../../include/overseer/WorkstationBar.h"

#include "DocumentArea.h"
#include "DocumentManager.h"
#include "FileWidget.h"
#include "Settings.h"
#include "TextDocument.h"

#include "../../include/ai/edit/EditSession.h"

#include "inference/InferenceService.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>

OverseerPage::OverseerPage(InferenceService *inferenceService,
                           EditSession *editSession, QWidget *parent)
    : QWidget(parent),
      m_inferenceService(inferenceService),
      m_editSession(editSession) {
  m_documentManager = new DocumentManager(this);

  m_overseer = new OverseerWidget(inferenceService, this);
  m_workstation = new Workstation(m_documentManager, editSession, this);

  m_documentArea = new DocumentArea(m_documentManager, this);
  m_documentArea->setEditSession(editSession);
  m_documentArea->hide();

  m_fileWidget = new FileWidget(this);

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

            m_workstation->openFile(absolutePath);
          });

  if (auto *op = m_overseer->sidePanel()->overviewPanel()) {
    connect(op, &OverviewPanel::openRequested, this,
            [this](const QString &relativePath) {
              if (m_currentOutputFolder.isEmpty())
                return;

              const QString abs =
                  PathUtils::toAbsolute(relativePath, m_currentOutputFolder);

              if (QFileInfo::exists(abs))
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

  // Left column: session list + session output tree.
  auto *leftColumn = new QSplitter(Qt::Vertical, this);
  leftColumn->addWidget(m_overseer->sessionListPanel());
  leftColumn->addWidget(m_fileWidget);

  // Center column: Workstation bar + Workstation on top, transcript below.
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

  m_mainSplitter = new QSplitter(Qt::Horizontal, this);
  m_mainSplitter->addWidget(leftColumn);
  m_mainSplitter->addWidget(centerColumn);
  m_mainSplitter->addWidget(m_overseer->sidePanel());
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);
  m_mainSplitter->setStretchFactor(2, 0);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->addWidget(m_mainSplitter);
}

OverseerPage::~OverseerPage() = default;

void OverseerPage::setThemeTokens(const ThemeTokens &tokens) {
  if (m_documentArea)
    m_documentArea->setThemeTokens(tokens);
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

  if (m_workstation)
    m_workstation->closeAll();

  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

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

  // 1. Destroy workstation windows synchronously.
  if (m_workstation)
    m_workstation->closeAll();

  // 2. Flush any deferred deletes so bodies are gone.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

  // 3. Now close documents safely.
  closeAllSessionDocuments();

  m_currentSessionName = name;
  m_currentOutputFolder = session->outputPath();

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

  if (m_workstation)
    m_workstation->openFile(dest);

  emit statusMessage(tr("Staged %1 in the session.").arg(info.fileName()),
                     3000);
}