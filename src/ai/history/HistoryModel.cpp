#include "../../../include/ai/history/HistoryModel.h"

HistoryModel::HistoryModel(QObject *parent) : QObject(parent) {}

void HistoryModel::clear() {
  if (m_entries.isEmpty()) {
    return;
  }

  m_entries.clear();
  emit changed();
}

void HistoryModel::append(const HistoryEntry &entry) {
  m_entries.append(entry);
  emit changed();
}

void HistoryModel::appendMany(const QVector<HistoryEntry> &entries) {
  if (entries.isEmpty()) {
    return;
  }

  for (const HistoryEntry &entry : entries) {
    m_entries.append(entry);
  }

  emit changed();
}