#pragma once

#include "ConductorRoster.h"
#include "inference/InferenceService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class InferenceService;
class ToolRegistry;
class PayloadLogger;

class FileAgent : public QObject {
  Q_OBJECT

public:
  struct Task {
    QString id;
    QString requestId;
    QString instruction;
  };

  struct DelegateToScopedEdit {
    QString filePath;
    QString instruction;
    QString requestId;
    QString taskId;
  };

  FileAgent(const QString &id, const QString &domain,
            InferenceService *inferenceService,
            ToolRegistry *tools,
            PayloadLogger *logger,
            QObject *parent = nullptr);

  ~FileAgent() override;

  QString id() const { return m_id; }
  QString domain() const { return m_domain; }

  bool isBusy() const { return m_busy; }
  int queueDepth() const { return m_queue.size(); }

  // The id of the task currently in flight, or an empty string when
  // the agent is idle.
  QString activeTaskId() const { return m_current.id; }

  QVector<Task> queue() const { return m_queue; }

  void setOutputFolder(const QString &folder) { m_outputFolder = folder; }

  // Per-agent ceiling on the number of inference turns this agent will
  // ever dispatch in its lifetime. Guards against a cyclic agent that
  // never reaches a terminal action. Normal tasks use a handful of
  // turns; the default is high enough that only a runaway agent hits
  // it. Raising the limit from the UI does not clear the counter; the
  // agent resumes dispatching as soon as the limit is above the count.
  void setToolCallDepthLimit(int limit);
  int toolCallDepthLimit() const { return m_toolCallDepthLimit; }
  int toolCallCount() const { return m_toolCallCount; }

  void enqueue(const Task &task);

  bool cancel(const QString &taskId);

  void finishTask(const QString &taskId, bool ok,
                  const QString &resultOrError);

  ConductorWorker rosterEntry() const;

  QString summaryForConductor() const;

  QStringList history() const { return m_history; }
  void setHistory(const QStringList &history);

  QStringList filesSeen() const { return m_filesSeen; }
  void setFilesSeen(const QStringList &files);

signals:
  void taskFinished(const QString &taskId, bool ok,
                    const QString &resultOrError);

  void delegateToScopedEdit(const FileAgent::DelegateToScopedEdit &delegate);

  void stateChanged();

  // Emitted around the execution of a write_file tool call so the
  // scheduler can record which agent owns the write on a path and
  // route subsequent readers of that path to this agent. The claim
  // is task-scoped: it is held for the duration of the task that
  // contains the write, across every turn of that task, and is
  // released when the task finishes.
  void fileWriteClaimed(const QString &taskId, const QString &relativePath);
  void fileWriteReleased(const QString &taskId, const QString &relativePath);

  // Emitted when the per-agent turn ceiling is hit. The agent stops
  // dispatching and fails the current task. The runner listens and
  // surfaces this to the user.
  void depthLimitReached(const QString &agentId, int limit);

private slots:
  void onResponseFinished(const InferenceService::RequestToken &token);
  void onResponseDelta(const InferenceService::RequestToken &token,
                       const QString &text);
  void onResponseError(const InferenceService::RequestToken &token,
                       const QString &error);

private:
  // Take the next task from the queue and mark the agent busy.
  void beginNextTask();

  // Send a request for the current task. Called once per turn.
  void dispatchTurn();

  // Schedule beginNextTask on the next event-loop turn.
  void scheduleDispatch();

  void finishCurrent(bool ok, const QString &resultOrError);
  void applyToolCall(const QJsonObject &toolCall);
  void appendHistory(const QString &line);

  QString buildPrompt(const Task &task) const;

  void logAgent(const QString &tag, const QString &content);

  // True if `token` belongs to the request currently being served.
  bool isCurrentToken(const InferenceService::RequestToken &token) const;

  // Release any write claims held for the current task.
  void releaseTaskClaims();

  QString m_id;
  QString m_domain;
  QString m_outputFolder;

  InferenceService *m_inferenceService = nullptr;
  ToolRegistry *m_tools = nullptr;
  PayloadLogger *m_logger = nullptr;

  QVector<Task> m_queue;
  Task m_current;
  bool m_busy = false;

  InferenceService::RequestToken m_activeToken;
  bool m_pendingDispatch = false;
  bool m_hasActiveToken = false;

  QString m_activeTaskId;
  QString m_accumulated;

  // Every tool call made during the current task, in order, each
  // with its recorded result. Rendered into the prompt on every turn
  // after the first, and cleared by finishCurrent.
  QVector<QJsonObject> m_taskToolCalls;

  // Relative paths written during the current task. Claims are
  // emitted on write and released when the task finishes.
  QStringList m_taskWriteClaims;

  QStringList m_history;
  QStringList m_filesSeen;

  // Per-agent turn ceiling and the running count. The count is never
  // reset per task; it is a lifetime total for this agent.
  int m_toolCallDepthLimit = 64;
  int m_toolCallCount = 0;

  static constexpr int kMaxHistory = 20;
  static constexpr int kMaxParseRetries = 1;
};