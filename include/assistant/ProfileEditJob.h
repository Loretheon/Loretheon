#pragma once

#include <QObject>
#include <QString>

class EditPlanner;
class EditSession;
class InferenceService;
class TextDocument;

// A scoped edit against one of the assistant's own files — identity.md,
// user.md, self.md, or a topic file under memories/topics/.
//
// The target file is loaded into a private TextDocument that is never
// shown in any editor. The scoped-edit pipeline (EditPlanner,
// EditSession, EditApplier) runs against that document exactly as it
// runs against a note, and the result is written back to disk with
// QSaveFile when the edit completes.
//
// No user review. The pipeline's accept-all + apply-all path is used
// unconditionally. This is deliberate: the assistant is editing its
// own memory, not the user's notes.
//
// A profile edit job is one-shot. It starts, runs one planner pass,
// applies, writes, and reports. It does not retry.
class ProfileEditJob : public QObject {
  Q_OBJECT

public:
  ProfileEditJob(InferenceService *inference, const QString &absolutePath,
                 const QString &instruction, QObject *parent = nullptr);
  ~ProfileEditJob() override;

  QString path() const { return m_path; }
  QString instruction() const { return m_instruction; }

  bool isRunning() const { return m_running; }

  void start();
  void abort();

  signals:
    void started();
  void finished(const QString &summary);
  void failed(const QString &reason);

private:
  void beginPlanning();
  void onGenerationFinished();
  void writeBackAndFinish();
  void reportFailure(const QString &reason);
  void detachPipeline();

  InferenceService *m_inference = nullptr;

  QString m_path;
  QString m_instruction;

  TextDocument *m_document = nullptr;

  EditPlanner *m_planner = nullptr;
  EditSession *m_session = nullptr;

  bool m_running = false;
  bool m_failed = false;
  bool m_reported = false;
  bool m_detached = false;
};