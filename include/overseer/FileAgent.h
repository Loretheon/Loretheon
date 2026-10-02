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

  QString activeTaskId() const { return m_current.id; }

  QVector<Task> queue() const { return m_queue; }

  void setOutputFolder(const QString &folder) { m_outputFolder = folder; }

  // Facts the agent may draw on when it needs to know something about
  // the user. Rendered into the agent's prompt. Set by the runner
  // from the session's global and session memory. Passing an empty
  // list clears the section.
  void setMemoryFacts(const QStringList &globalFacts,
                      const QStringList &sessionFacts);

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

  void fileWriteClaimed(const QString &taskId, const QString &relativePath);
  void fileWriteReleased(const QString &taskId, const QString &relativePath);

  void depthLimitReached(const QString &agentId, int limit);

private slots:
  void onResponseFinished(const InferenceService::RequestToken &token);
  void onResponseDelta(const InferenceService::RequestToken &token,
                       const QString &text);
  void onResponseError(const InferenceService::RequestToken &token,
                       const QString &error);

private:
  void beginNextTask();
  void dispatchTurn();
  void scheduleDispatch();

  void finishCurrent(bool ok, const QString &resultOrError);
  void applyToolCall(const QJsonObject &toolCall);
  void appendHistory(const QString &line);

  QString buildPrompt(const Task &task) const;

  void logAgent(const QString &tag, const QString &content);

  bool isCurrentToken(const InferenceService::RequestToken &token) const;

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

  QVector<QJsonObject> m_taskToolCalls;

  QStringList m_taskWriteClaims;

  QStringList m_history;
  QStringList m_filesSeen;

  // Facts the agent may draw on. Two lists, matching the two memory
  // scopes.
  QStringList m_globalFacts;
  QStringList m_sessionFacts;

  int m_toolCallDepthLimit = 64;
  int m_toolCallCount = 0;

  static constexpr int kMaxHistory = 20;
  static constexpr int kMaxParseRetries = 1;
};