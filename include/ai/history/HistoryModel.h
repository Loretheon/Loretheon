#pragma once

#include "../edit/EditCommand.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

struct HistoryEntry {
  QString userRequest;
  EditCommand command;
  QString outcome;  // "Applied", "Rejected", "Failed", "Fuzzy"
  QString detail;
  qint64 timestampMs = 0;
};

class HistoryModel : public QObject {
  Q_OBJECT

public:
  explicit HistoryModel(QObject *parent = nullptr);

  void clear();

  void append(const HistoryEntry &entry);
  void appendMany(const QVector<HistoryEntry> &entries);

  const QList<HistoryEntry> &entries() const { return m_entries; }

  signals:
    void changed();

private:
  QList<HistoryEntry> m_entries;
};