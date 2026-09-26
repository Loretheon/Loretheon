#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// Maps an Oculus 15 viseme name (the names QF-ML's TtsManager emits)
// onto one or more Reallusion shape key weights on the CC Base face
// mesh. This is separate from QF-ML's include/voice/VisemeMap.h,
// which maps the Oculus codes to themselves; this maps them to
// morph target indices on the Lore avatar.
class VisemeTable {
public:
  struct Weight {
    int morphIndex = -1;
    float weight = 0.0f;
  };

  void build(const QHash<QString, int> &nameToIndex);

  QVector<Weight> weightsFor(const QString &viseme) const;

  int targetCount() const { return m_targetCount; }

  QStringList knownVisemes() const;

private:
  QHash<QString, QVector<Weight>> m_table;
  int m_targetCount = 0;
};