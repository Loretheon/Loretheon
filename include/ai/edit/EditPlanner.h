#ifndef EDITPLANNER_H
#define EDITPLANNER_H

#include "PayloadLogger.h"
#include "edit/EditCommand.h"

#include "inference/InferenceService.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

class TextEdit;
class TextDocument;

class EditPlanner : public QObject {
  Q_OBJECT

public:
  enum class ScopeMode {
    Scoped,
    WholeFile,
  };

  Q_ENUM(ScopeMode)

  explicit EditPlanner(InferenceService *inferenceService,
                       QObject *parent = nullptr);

  // Editor-bound entry points, used by normal mode. The editor's
  // document is used as the planning target.
  void start(TextEdit *editor, const QString &userRequest);
  void start(TextEdit *editor, const QString &userRequest, ScopeMode mode);

  // Document-bound entry points, used by Overseer mode. The document
  // does not need to be displayed in any editor.
  void start(TextDocument *document, const QString &userRequest);
  void start(TextDocument *document, const QString &userRequest,
             ScopeMode mode);

  void abort();

  bool isActive() const { return m_active; }

  // How long the planner will wait for a complete plan before it
  // gives up and emits failed(). Set in startOnDocument() to a value
  // comfortably above the LLM timeout passed to sendChatRequest, so
  // the LLM's own timeout fires first in normal operation and this
  // one only catches the case where the LLM layer never reports
  // anything at all.
  int watchdogMs() const { return m_watchdogMs; }
  void setWatchdogMs(int ms);

signals:
  void planValidated(const QVector<EditCommand> &commands);

  void contextScopes(const QStringList &scopeIds);

  void failed(const QString &reason);

private:
  void startOnDocument(TextDocument *document, const QString &userRequest,
                       ScopeMode mode);

  void processStream();

  bool takeCompleteJsonValue(QString &buffer, QString &jsonText);

  void armWatchdog();
  void disarmWatchdog();

  InferenceService *m_inferenceService{nullptr};

  // Retained only for lifetime; the planner does not use it.
  TextEdit *m_editor{nullptr};

  TextDocument *m_document{nullptr};

  QString m_userRequest;

  QString m_streamingResponse;

  PayloadLogger m_payloadLogger;

  ScopeMode m_scopeMode{ScopeMode::Scoped};

  bool m_active{false};

  InferenceService::RequestToken m_activeToken;

  // Fires if no planValidated and no failed is emitted within
  // m_watchdogMs of startOnDocument. Guards against an LLM layer
  // that stalls without ever reporting llmFinished or llmError.
  QTimer *m_watchdog{nullptr};

  int m_watchdogMs{150000};

  // Guards against emitting failed() twice for the same start.
  // Set to true the moment the planner emits either planValidated
  // or failed. Cleared at the top of startOnDocument.
  bool m_terminalEmitted{false};
};

#endif // EDITPLANNER_H