#include "../../include/assistant/ProfileEditJob.h"

#include "../../include/ai/edit/EditCommand.h"
#include "../../include/ai/edit/EditPlanner.h"
#include "../../include/ai/edit/EditSession.h"
#include "../../include/text/model/TextDocument.h"
#include "DocumentMode.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>

#define PEJ_LOG qDebug() << "[ProfileEditJob]"

ProfileEditJob::ProfileEditJob(InferenceService *inference,
                               const QString &absolutePath,
                               const QString &instruction, QObject *parent)
    : QObject(parent), m_inference(inference), m_path(absolutePath),
      m_instruction(instruction) {
  PEJ_LOG << "ctor this=" << this << "path=" << m_path;
}

ProfileEditJob::~ProfileEditJob() {
  PEJ_LOG << "dtor this=" << this << "detached=" << m_detached;
  detachPipeline();
}

void ProfileEditJob::detachPipeline() {
  if (m_detached) {
    return;
  }

  m_detached = true;

  if (m_planner) {
    disconnect(m_planner, nullptr, this, nullptr);
  }

  if (m_session) {
    disconnect(m_session, nullptr, this, nullptr);
  }
}

void ProfileEditJob::start() {
  PEJ_LOG << "start this=" << this << "running=" << m_running;

  if (m_running) {
    return;
  }

  if (!m_inference) {
    reportFailure(tr("The edit pipeline is unavailable."));
    return;
  }

  if (m_path.isEmpty() || !QFileInfo::exists(m_path)) {
    reportFailure(tr("The file does not exist: %1").arg(m_path));
    return;
  }

  if (m_instruction.trimmed().isEmpty()) {
    reportFailure(tr("The instruction is empty."));
    return;
  }

  m_running = true;
  m_failed = false;
  m_reported = false;

  QTimer::singleShot(0, this, [this]() { beginPlanning(); });
}

void ProfileEditJob::beginPlanning() {
  if (!m_running || m_failed) {
    return;
  }

  // Load the file into a private TextDocument. No editor, no
  // DocumentManager. The document is owned by this job.
  QString text;

  {
    QFile file(m_path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      reportFailure(tr("Could not read %1: %2")
                        .arg(QFileInfo(m_path).fileName(),
                             file.errorString()));
      return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    text = stream.readAll();

    if (stream.status() != QTextStream::Ok) {
      reportFailure(tr("Could not read %1.")
                        .arg(QFileInfo(m_path).fileName()));
      return;
    }
  }

  m_document = new TextDocument(this);
  m_document->setFilePath(m_path);
  m_document->setType(DocumentMode::Markdown);
  m_document->setPlainText(text);
  m_document->rebuildStructure();

  m_session = EditSession::forDocument(m_document, this);
  m_session->setInferenceService(m_inference);

  m_planner = new EditPlanner(m_inference, this);

  connect(m_planner, &EditPlanner::planValidated, this,
          [this](const QVector<EditCommand> &commands) {
            if (!m_running || m_failed) {
              return;
            }

            if (!m_session) {
              reportFailure(tr("The edit session disappeared."));
              return;
            }

            if (commands.isEmpty()) {
              reportFailure(tr("The planner produced no edits."));
              return;
            }

            if (!m_session->executePlan(commands)) {
              return;
            }

            m_session->startAllPendingEdits();
          });

  connect(m_planner, &EditPlanner::failed, this,
          [this](const QString &reason) {
            reportFailure(tr("Planning failed: %1").arg(reason));
          });

  connect(m_session, &EditSession::failed, this,
          [this](const QString &reason) {
            reportFailure(tr("Editing failed: %1").arg(reason));
          });

  connect(m_session, &EditSession::generationFinished, this,
          [this](bool allCompleted) {
            if (!m_running || m_failed) {
              return;
            }

            if (!allCompleted) {
              reportFailure(tr("Some edits did not finish generating."));
              return;
            }

            m_session->acceptAllPendingEdits();
            m_session->applyAcceptedPendingEdits();

            onGenerationFinished();
          });

  emit started();

  m_planner->start(m_document, m_instruction, EditPlanner::ScopeMode::Scoped);
}

void ProfileEditJob::onGenerationFinished() {
  if (!m_running || m_failed) {
    return;
  }

  if (!m_document) {
    reportFailure(tr("The document disappeared before the edits landed."));
    return;
  }

  m_document->rebuildStructure();

  writeBackAndFinish();
}

void ProfileEditJob::writeBackAndFinish() {
  const QString newText = m_document->toPlainText();

  QSaveFile file(m_path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    reportFailure(tr("Could not open %1 for writing: %2")
                      .arg(QFileInfo(m_path).fileName(),
                           file.errorString()));
    return;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << newText;
  stream.flush();

  if (stream.status() != QTextStream::Ok) {
    file.cancelWriting();
    reportFailure(tr("Could not write %1.")
                      .arg(QFileInfo(m_path).fileName()));
    return;
  }

  if (!file.commit()) {
    reportFailure(tr("Could not commit %1.")
                      .arg(QFileInfo(m_path).fileName()));
    return;
  }

  m_running = false;
  m_reported = true;

  detachPipeline();

  emit finished(tr("Edited %1: %2")
                    .arg(QFileInfo(m_path).fileName(), m_instruction));

  deleteLater();
}

void ProfileEditJob::abort() {
  if (!m_running) {
    return;
  }

  m_running = false;
  m_reported = true;

  detachPipeline();

  if (m_planner) {
    auto *planner = m_planner;
    QTimer::singleShot(0, planner, [planner]() { planner->abort(); });
  }

  if (m_session) {
    auto *session = m_session;
    QTimer::singleShot(0, session, [session]() { session->abort(); });
  }

  emit failed(tr("Cancelled."));

  deleteLater();
}

void ProfileEditJob::reportFailure(const QString &reason) {
  if (m_reported) {
    return;
  }

  m_reported = true;
  m_failed = true;
  m_running = false;

  detachPipeline();

  if (m_planner) {
    auto *planner = m_planner;
    QTimer::singleShot(0, planner, [planner]() { planner->abort(); });
  }

  if (m_session) {
    auto *session = m_session;
    QTimer::singleShot(0, session, [session]() { session->abort(); });
  }

  emit failed(reason);

  deleteLater();
}