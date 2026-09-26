#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

// The user's action stream. Every meaningful action the user takes is
// recorded here as a short event, and the assistant receives batches
// of them on a timer. She decides what, if anything, to do with each
// batch. Most batches produce nothing.
//
// The stream is not persisted. It lives for the session and is
// discarded on close. Only the assistant's summaries survive, in the
// session memory file.
class AssistantActivity : public QObject {
  Q_OBJECT

public:
  enum class Kind {
    FileOpened,
    FileClosed,
    FileSaved,
    FileEdited,
    ModeSwitched,
    SearchRun,
    ImportRun,
    SpeakInvoked,
    UserMessage,
    SessionStarted,
  };

  explicit AssistantActivity(QObject *parent = nullptr);
  ~AssistantActivity() override;

  // Record an event. The payload is a short human-readable string, not
  // structured data, because the assistant only ever sees it as text.
  void record(Kind kind, const QString &payload = QString());

  // The most recent N events, newest last.
  QStringList recent(int count = 20) const;

  // Start and stop the batch timer. Batches are emitted on the timer
  // when the buffer is non-empty and the assistant is allowed to act.
  void startBatchTimer();
  void stopBatchTimer();

  // How often a batch is delivered. Default 30 seconds.
  void setBatchIntervalMs(int ms);
  int batchIntervalMs() const;

  // True when there are unflushed events.
  bool hasPending() const { return !m_pending.isEmpty(); }

signals:
  // Emitted every batchIntervalMs with the events accumulated since
  // the last emission. Never emitted with an empty list.
  void batchReady(const QStringList &events);

private slots:
  void onBatchTick();

private:
  static QString formatEvent(Kind kind, const QString &payload,
                             const QDateTime &when);

  QVector<QString> m_all;      // every event this session
  QStringList m_pending;       // events not yet delivered
  QTimer *m_batchTimer = nullptr;
  int m_batchIntervalMs = 30000;
};