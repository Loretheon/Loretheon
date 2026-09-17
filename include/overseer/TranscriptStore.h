#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "TranscriptEvent.h"

class OverseerSession;

class TranscriptStore : public QObject {
  Q_OBJECT

public:
  explicit TranscriptStore(QObject *parent = nullptr);

  void setSession(OverseerSession *session);
  OverseerSession *session() const { return m_session; }

  const QList<TranscriptEvent> &events() const { return m_events; }

  void clear();
  void loadFromDisk();
  void append(const TranscriptEvent &event);

  void updateProposalStatus(const QString &proposalKey,
                            const QString &status,
                            const QString &acceptedScope = QString());

  void updatePlanStatus(const QString &planId, const QString &status,
                        const QString &result);

  signals:
    void eventsReset();
  void eventAppended(int index);
  void eventUpdated(int index);

private:
  QString transcriptPath() const;
  void rewriteDisk();

  OverseerSession *m_session = nullptr;
  QList<TranscriptEvent> m_events;
  quint64 m_sequence = 0;
};