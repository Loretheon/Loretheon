#pragma once

#include <QDialog>
#include <QString>

struct EditCommand;
class EditPlanner;
class EditSession;
class EditSessionWidget;
class InferenceService;
class PreviewPane;
class TextDocument;
class TextEdit;

class QLabel;
class QPushButton;

// Non-modal dialog that opens a session-local copy of a note, runs the
// standard edit pipeline against the given instruction, and lets the user
// review, apply, and optionally promote the changes back to the original.
//
// The dialog owns its TextDocument (loaded from the copy), TextEdit,
// EditSession, EditPlanner, and EditSessionWidget. Everything is scoped to
// this dialog; closing it discards the pipeline.
class EditNoteReviewDialog : public QDialog {
  Q_OBJECT

public:
  EditNoteReviewDialog(InferenceService *inferenceService,
                       const QString &copyPath,
                       const QString &originalPath,
                       const QString &instruction,
                       QWidget *parent = nullptr);

  ~EditNoteReviewDialog() override;

signals:
  // Emitted after the copy has been written to disk. `copyPath` is the
  // file that was saved; `originalPath` is the note the user may promote
  // to later.
  void copySaved(const QString &copyPath, const QString &originalPath);

private slots:
  void onSaveCopyClicked();
  void onPromoteClicked();
  void onPlannerValidated(const QVector<EditCommand> &commands);
  void onPlannerFailed(const QString &reason);
  void onSessionFailed(const QString &reason);

private:
  void applyTheme();

  InferenceService *m_inference = nullptr;

  QString m_copyPath;
  QString m_originalPath;
  QString m_instruction;

  TextDocument *m_document = nullptr;
  TextEdit *m_editor = nullptr;
  PreviewPane *m_preview = nullptr;

  EditSession *m_session = nullptr;
  EditPlanner *m_planner = nullptr;
  EditSessionWidget *m_sessionWidget = nullptr;

  QLabel *m_statusLabel = nullptr;
  QPushButton *m_saveButton = nullptr;
  QPushButton *m_promoteButton = nullptr;
  QPushButton *m_closeButton = nullptr;
};