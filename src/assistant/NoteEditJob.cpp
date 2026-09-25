#include "../../include/assistant/NoteEditJob.h"

#include "../../include/ai/edit/EditCommand.h"
#include "../../include/ai/edit/EditPlanner.h"
#include "../../include/ai/edit/EditSession.h"
#include "../../include/app/DocumentManager.h"
#include "../../include/text/DocumentArea.h"
#include "TextDocument.h"
#include "../../include/text/TextEdit.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QFileInfo>
#include <QTimer>

#define NEJ_LOG qDebug() << "[NoteEditJob]"

NoteEditJob::NoteEditJob(DocumentManager *documents,
                         DocumentArea *documentArea,
                         InferenceService *inference,
                         const QString &notePath,
                         const QString &instruction, QObject *parent)
    : QObject(parent), m_documents(documents), m_documentArea(documentArea),
      m_inference(inference), m_notePath(notePath),
      m_instruction(instruction) {
  NEJ_LOG << "ctor this=" << this << "path=" << m_notePath;
}

NoteEditJob::~NoteEditJob() {
  NEJ_LOG << "dtor this=" << this << "planner=" << m_planner
          << "session=" << m_session << "detached=" << m_detached;

  // Do NOT call planner->abort() or session->abort() here. Both
  // re-enter InferenceService and touch state that may already have
  // been torn down by the time the destructor runs. Disconnect is
  // enough: a disconnected child cannot deliver signals into this
  // object.
  detachPipeline();

  NEJ_LOG << "dtor done this=" << this;
}

void NoteEditJob::detachPipeline() {
  if (m_detached) {
    return;
  }

  m_detached = true;

  NEJ_LOG << "detachPipeline this=" << this;

  if (m_planner) {
    disconnect(m_planner, nullptr, this, nullptr);
  }

  if (m_session) {
    disconnect(m_session, nullptr, this, nullptr);
  }
}

void NoteEditJob::start() {
  NEJ_LOG << "start this=" << this << "running=" << m_running;

  if (m_running) {
    return;
  }

  if (!m_documents || !m_documentArea || !m_inference) {
    reportFailure(tr("The edit pipeline is unavailable."));
    return;
  }

  if (m_notePath.isEmpty() || !QFileInfo::exists(m_notePath)) {
    reportFailure(tr("The note does not exist: %1").arg(m_notePath));
    return;
  }

  if (m_instruction.trimmed().isEmpty()) {
    reportFailure(tr("The instruction is empty."));
    return;
  }

  m_running = true;
  m_failed = false;
  m_reported = false;

  QTimer::singleShot(0, this, [this]() { openNote(); });
}

void NoteEditJob::openNote() {
  NEJ_LOG << "openNote this=" << this << "running=" << m_running
          << "failed=" << m_failed;

  if (!m_running || m_failed) {
    return;
  }

  if (!m_documents->openFile(m_notePath)) {
    reportFailure(tr("Could not open the note for editing."));
    return;
  }

  QTimer::singleShot(0, this, [this]() {
    NEJ_LOG << "openNote(deferred) this=" << this << "running=" << m_running
            << "failed=" << m_failed;

    if (!m_running || m_failed) {
      return;
    }

    m_editor = m_documentArea->currentEditor();

    NEJ_LOG << "openNote editor=" << m_editor;

    if (!m_editor) {
      reportFailure(tr("The note was opened but no editor is active."));
      return;
    }

    auto *document = qobject_cast<TextDocument *>(m_editor->document());

    NEJ_LOG << "openNote document=" << document;

    if (!document) {
      reportFailure(tr("The active editor does not use TextDocument."));
      return;
    }

    const QString openPath =
        QFileInfo(document->filePath()).absoluteFilePath();
    const QString wantedPath = QFileInfo(m_notePath).absoluteFilePath();

    NEJ_LOG << "openNote openPath=" << openPath
            << "wantedPath=" << wantedPath;

    if (openPath != wantedPath) {
      reportFailure(
          tr("The editor opened a different document than the note."));
      return;
    }

    beginPlanning();
  });
}

void NoteEditJob::beginPlanning() {
  NEJ_LOG << "beginPlanning this=" << this << "editor=" << m_editor;

  if (!m_running || m_failed) {
    return;
  }

  m_session = new EditSession(m_editor, this);
  NEJ_LOG << "beginPlanning session=" << m_session;

  m_session->setInferenceService(m_inference);

  m_planner = new EditPlanner(m_inference, this);
  NEJ_LOG << "beginPlanning planner=" << m_planner;

  connect(m_planner, &EditPlanner::planValidated, this,
          [this](const QVector<EditCommand> &commands) {
            NEJ_LOG << "planValidated this=" << this
                    << "commands=" << commands.size()
                    << "running=" << m_running
                    << "failed=" << m_failed;

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

            NEJ_LOG << "executePlan begin";
            if (!m_session->executePlan(commands)) {
              NEJ_LOG << "executePlan returned false";
              return;
            }
            NEJ_LOG << "executePlan returned true";

            auto *document =
                qobject_cast<TextDocument *>(m_editor->document());

            if (document) {
              m_revisionBeforeApply = document->revision();
              NEJ_LOG << "revisionBeforeApply=" << m_revisionBeforeApply;
            }

            NEJ_LOG << "startAllPendingEdits begin";
            m_session->startAllPendingEdits();
            NEJ_LOG << "startAllPendingEdits returned";
          });

  connect(m_planner, &EditPlanner::failed, this,
          [this](const QString &reason) {
            NEJ_LOG << "planner failed reason=" << reason;
            reportFailure(tr("Planning failed: %1").arg(reason));
          });

  connect(m_session, &EditSession::failed, this,
          [this](const QString &reason) {
            NEJ_LOG << "session failed reason=" << reason;
            reportFailure(tr("Editing failed: %1").arg(reason));
          });

  connect(m_session, &EditSession::generationFinished, this,
          [this](bool allCompleted) {
            NEJ_LOG << "generationFinished this=" << this
                    << "allCompleted=" << allCompleted
                    << "running=" << m_running
                    << "failed=" << m_failed;

            if (!m_running || m_failed) {
              return;
            }

            if (!allCompleted) {
              reportFailure(tr("Some edits did not finish generating."));
              return;
            }

            NEJ_LOG << "acceptAllPendingEdits begin";
            m_session->acceptAllPendingEdits();
            NEJ_LOG << "applyAcceptedPendingEdits begin";
            m_session->applyAcceptedPendingEdits();
            NEJ_LOG << "applyAcceptedPendingEdits returned";

            onGenerationFinished();
          });

  NEJ_LOG << "emit started";
  emit started();

  NEJ_LOG << "planner->start begin";
  m_planner->start(m_editor, m_instruction, EditPlanner::ScopeMode::Scoped);
  NEJ_LOG << "planner->start returned";
}

void NoteEditJob::onGenerationFinished() {
  NEJ_LOG << "onGenerationFinished this=" << this;

  if (!m_running || m_failed) {
    return;
  }

  auto *document = qobject_cast<TextDocument *>(m_editor->document());

  NEJ_LOG << "onGenerationFinished document=" << document;

  if (!document) {
    reportFailure(tr("The document disappeared before the edits landed."));
    return;
  }

  NEJ_LOG << "revisionBeforeApply=" << m_revisionBeforeApply
          << "revisionNow=" << document->revision();

  if (m_revisionBeforeApply >= 0 &&
      document->revision() == m_revisionBeforeApply) {
    reportFailure(
        tr("The edits were generated but none were applied to the note."));
    return;
  }

  m_running = false;
  m_reported = true;

  NEJ_LOG << "emit finished";

  // Detach before emitting so that any queued signal from the planner
  // or session that fires during the emit cannot re-enter this object.
  detachPipeline();

  emit finished(
      tr("Edited %1: %2")
          .arg(QFileInfo(m_notePath).fileName(), m_instruction));

  NEJ_LOG << "deleteLater begin";
  deleteLater();
  NEJ_LOG << "deleteLater returned";
}

void NoteEditJob::abort() {
  NEJ_LOG << "abort this=" << this << "running=" << m_running;

  if (!m_running) {
    return;
  }

  m_running = false;
  m_reported = true;

  // Detach first. Then ask the planner and session to stop, on the
  // event loop, after this call returns. Do not call abort() on them
  // synchronously: EditPlanner::abort() re-enters InferenceService,
  // and doing that from inside a slot that may already be inside a
  // shared teardown path is what crashed.
  detachPipeline();

  if (m_planner) {
    NEJ_LOG << "abort scheduler planner->abort";
    auto *planner = m_planner;
    QTimer::singleShot(0, planner, [planner]() {
      NEJ_LOG << "deferred planner->abort";
      planner->abort();
    });
  }

  if (m_session) {
    NEJ_LOG << "abort scheduler session->abort";
    auto *session = m_session;
    QTimer::singleShot(0, session, [session]() {
      NEJ_LOG << "deferred session->abort";
      session->abort();
    });
  }

  emit failed(tr("Cancelled."));

  NEJ_LOG << "abort deleteLater";
  deleteLater();
}

void NoteEditJob::reportFailure(const QString &reason) {
  NEJ_LOG << "reportFailure this=" << this << "reported=" << m_reported
          << "reason=" << reason;

  if (m_reported) {
    return;
  }

  m_reported = true;
  m_failed = true;
  m_running = false;

  detachPipeline();

  if (m_planner) {
    NEJ_LOG << "reportFailure scheduler planner->abort";
    auto *planner = m_planner;
    QTimer::singleShot(0, planner, [planner]() {
      NEJ_LOG << "deferred planner->abort";
      planner->abort();
    });
  }

  if (m_session) {
    NEJ_LOG << "reportFailure scheduler session->abort";
    auto *session = m_session;
    QTimer::singleShot(0, session, [session]() {
      NEJ_LOG << "deferred session->abort";
      session->abort();
    });
  }

  NEJ_LOG << "reportFailure emit failed";
  emit failed(reason);

  NEJ_LOG << "reportFailure deleteLater";
  deleteLater();
  NEJ_LOG << "reportFailure deleteLater returned";
}