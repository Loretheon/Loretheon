#pragma once

#include <QObject>
#include <QString>

class DocumentArea;
class DocumentManager;
class EditPlanner;
class EditSession;
class InferenceService;
class TextEdit;

class NoteEditJob : public QObject {
  Q_OBJECT

public:
  NoteEditJob(DocumentManager *documents, DocumentArea *documentArea,
              InferenceService *inference, const QString &notePath,
              const QString &instruction, QObject *parent = nullptr);
  ~NoteEditJob() override;

  QString notePath() const { return m_notePath; }

  bool isRunning() const { return m_running; }

  void start();
  void abort();

  signals:
    void started();
  void finished(const QString &summary);
  void failed(const QString &reason);

private:
  void openNote();
  void beginPlanning();
  void onGenerationFinished();
  void reportFailure(const QString &reason);

  void detachPipeline();

  DocumentManager *m_documents = nullptr;
  DocumentArea *m_documentArea = nullptr;
  InferenceService *m_inference = nullptr;

  QString m_notePath;
  QString m_instruction;

  TextEdit *m_editor = nullptr;

  EditPlanner *m_planner = nullptr;
  EditSession *m_session = nullptr;

  int m_revisionBeforeApply = -1;

  bool m_running = false;
  bool m_failed = false;
  bool m_reported = false;
  bool m_detached = false;
};