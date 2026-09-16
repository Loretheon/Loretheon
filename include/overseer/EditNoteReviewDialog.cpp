#include "../../include/overseer/EditNoteReviewDialog.h"

#include "../../include/ai/edit/EditSession.h"
#include "../../include/ai/edit/EditSessionWidget.h"
#include "../../include/ai/edit/EditPlanner.h"

#include "inference/InferenceService.h"

#include "TextDocument.h"
#include "TextEdit.h"
#include "preview/PreviewPane.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSplitter>
#include <QTextStream>
#include <QVBoxLayout>

EditNoteReviewDialog::EditNoteReviewDialog(InferenceService *inferenceService,
                                           const QString &copyPath,
                                           const QString &originalPath,
                                           const QString &instruction,
                                           QWidget *parent)
    : QDialog(parent),
      m_inference(inferenceService),
      m_copyPath(copyPath),
      m_originalPath(originalPath),
      m_instruction(instruction) {
  setWindowTitle(tr("Edit note: %1").arg(QFileInfo(copyPath).fileName()));
  resize(1200, 800);
  setModal(false);

  // --- Load copy into a fresh document -----------------------------------

  m_document = new TextDocument(this);

  {
    QFile file(copyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream stream(&file);
      stream.setEncoding(QStringConverter::Utf8);
      m_document->setPlainText(stream.readAll());
    }
  }

  m_document->setFilePath(copyPath);
  m_document->setModified(false);

  m_editor = new TextEdit(this);
  m_editor->setDocument(m_document);

  // --- Preview pane -------------------------------------------------------

  m_preview = new PreviewPane(this);

  auto *editorSplitter = new QSplitter(Qt::Horizontal, this);
  editorSplitter->addWidget(m_editor);
  editorSplitter->addWidget(m_preview);
  editorSplitter->setSizes({700, 400});

  // --- Session and widget -------------------------------------------------

  m_session = new EditSession(m_editor, this);
  m_sessionWidget = new EditSessionWidget(this);

  connect(m_sessionWidget, &EditSessionWidget::pendingEditAccepted,
          m_session, &EditSession::acceptPendingEdit);

  connect(m_sessionWidget, &EditSessionWidget::pendingEditRejected,
          m_session, &EditSession::rejectPendingEdit);

  connect(m_sessionWidget, &EditSessionWidget::acceptAllPendingEditsRequested,
          m_session, &EditSession::acceptAllPendingEdits);

  connect(m_sessionWidget, &EditSessionWidget::rejectAllPendingEditsRequested,
          m_session, &EditSession::rejectAllPendingEdits);

  connect(m_sessionWidget,
          &EditSessionWidget::applyAcceptedPendingEditsRequested, this,
          [this]() {
            m_session->applyAcceptedPendingEdits();
            m_document->setModified(true);
            m_statusLabel->setText(tr("Applied."));
          });

  connect(m_session, &EditSession::pendingEditStarted, this,
          [this](const PendingEdit &edit) {
            m_sessionWidget->startEdit(edit.id, edit.command.instruction);
            m_sessionWidget->setStatus(edit.id, tr("Writing"));
          });

  connect(m_session, &EditSession::pendingEditUpdated, this,
          [this](const PendingEdit &edit) {
            m_sessionWidget->setResultText(edit.id, edit.generatedText);
          });

  connect(m_session, &EditSession::pendingEditFinished, this,
          [this](const PendingEdit &edit) {
            m_sessionWidget->setResultText(edit.id, edit.generatedText);
            m_sessionWidget->setPendingEditReady(edit.id);
          });

  connect(m_session, &EditSession::planValidated, this,
          &EditNoteReviewDialog::onPlannerValidated);

  connect(m_session, &EditSession::failed, this,
          &EditNoteReviewDialog::onSessionFailed);

  // --- Planner ------------------------------------------------------------

  if (m_inference) {
    m_planner = new EditPlanner(m_inference, this);

    connect(m_planner, &EditPlanner::planValidated, this,
            &EditNoteReviewDialog::onPlannerValidated);

    connect(m_planner, &EditPlanner::failed, this,
            &EditNoteReviewDialog::onPlannerFailed);
  }

  // --- Actions ------------------------------------------------------------

  m_statusLabel = new QLabel(tr("Planning…"), this);

  m_saveButton = new QPushButton(tr("Save copy"), this);
  m_promoteButton = new QPushButton(tr("Promote to notes root"), this);
  m_closeButton = new QPushButton(tr("Close"), this);

  connect(m_saveButton, &QPushButton::clicked, this,
          &EditNoteReviewDialog::onSaveCopyClicked);

  connect(m_promoteButton, &QPushButton::clicked, this,
          &EditNoteReviewDialog::onPromoteClicked);

  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);

  auto *buttonRow = new QHBoxLayout;
  buttonRow->addWidget(m_statusLabel, 1);
  buttonRow->addWidget(m_saveButton);
  buttonRow->addWidget(m_promoteButton);
  buttonRow->addWidget(m_closeButton);

  // --- Layout -------------------------------------------------------------

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(6);

  auto *instructionLabel = new QLabel(
      tr("<b>Instruction:</b> %1").arg(instruction.toHtmlEscaped()), this);
  instructionLabel->setWordWrap(true);
  root->addWidget(instructionLabel);

  root->addWidget(editorSplitter, 1);
  root->addWidget(m_sessionWidget, 1);
  root->addLayout(buttonRow);

  // --- Kick off planning --------------------------------------------------

  if (m_planner) {
    m_planner->start(m_editor, instruction);
  } else {
    m_statusLabel->setText(tr("Inference service unavailable."));
    m_saveButton->setEnabled(false);
    m_promoteButton->setEnabled(false);
  }
}

EditNoteReviewDialog::~EditNoteReviewDialog() = default;


namespace {
QString opToString(EditCommand::Operation op) {
  switch (op) {
  case EditCommand::Operation::Insert: return QStringLiteral("insert");
  case EditCommand::Operation::Replace: return QStringLiteral("replace");
  case EditCommand::Operation::ReplaceScope: return QStringLiteral("replace_scope");
  case EditCommand::Operation::Delete: return QStringLiteral("delete");
  case EditCommand::Operation::Unknown: break;
  }
  return QStringLiteral("unknown");
}

QString posToString(EditCommand::Position pos) {
  switch (pos) {
  case EditCommand::Position::Before: return QStringLiteral("before");
  case EditCommand::Position::After: return QStringLiteral("after");
  case EditCommand::Position::Inside: return QStringLiteral("inside");
  }
  return QStringLiteral("inside");
}
} // namespace


void EditNoteReviewDialog::onPlannerValidated(
    const QVector<EditCommand> &commands) {
  if (!m_session) {
    return;
  }

  if (!m_session->executePlan(commands)) {
    return;
  }

  m_statusLabel->setText(tr("Awaiting review."));
}

void EditNoteReviewDialog::onPlannerFailed(const QString &reason) {
  m_statusLabel->setText(tr("Planning failed: %1").arg(reason));
  m_saveButton->setEnabled(false);
  m_promoteButton->setEnabled(false);
}

void EditNoteReviewDialog::onSessionFailed(const QString &reason) {
  m_statusLabel->setText(tr("Error: %1").arg(reason));
}

void EditNoteReviewDialog::onSaveCopyClicked() {
  QSaveFile file(m_copyPath);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Save copy"),
                         tr("Could not write %1.").arg(m_copyPath));
    return;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << m_document->toPlainText();
  stream.flush();

  if (stream.status() != QTextStream::Ok || !file.commit()) {
    QMessageBox::warning(this, tr("Save copy"),
                         tr("Failed while writing %1.").arg(m_copyPath));
    return;
  }

  m_document->setModified(false);
  m_statusLabel->setText(tr("Saved copy."));

  emit copySaved(m_copyPath, m_originalPath);
}

void EditNoteReviewDialog::onPromoteClicked() {
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Promote to notes root"),
      tr("Overwrite\n%1\nwith the session copy?\n\nThis cannot be undone.")
          .arg(m_originalPath),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    return;
  }

  // Save the copy first so what we promote is what's on screen.
  onSaveCopyClicked();

  QFile source(m_copyPath);
  QFile target(m_originalPath);

  if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Promote"),
                         tr("Could not read %1.").arg(m_copyPath));
    return;
  }

  QSaveFile out(m_originalPath);

  if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Promote"),
                         tr("Could not open %1 for writing.")
                             .arg(m_originalPath));
    return;
  }

  QTextStream stream(&out);
  stream.setEncoding(QStringConverter::Utf8);
  stream << QString::fromUtf8(source.readAll());
  stream.flush();

  if (stream.status() != QTextStream::Ok || !out.commit()) {
    QMessageBox::warning(this, tr("Promote"),
                         tr("Failed while writing %1.").arg(m_originalPath));
    return;
  }

  m_statusLabel->setText(tr("Promoted to notes root."));
}