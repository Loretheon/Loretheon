#include "../../include/avatar/VisemeTable.h"

#include <QDebug>

namespace {

struct Spec {
  const char *viseme;
  const char *shape;
  float weight;
};

constexpr Spec kSpec[] = {
    {"PP", "Mouth_Close", 0.85f},
    {"FF", "Mouth_Close", 0.45f},
    {"TH", "V_Dental_Lip", 0.85f},
    {"DD", "Mouth_Close", 0.30f},
    {"DD", "Jaw_Open", 0.15f},
    {"kk", "Mouth_Close", 0.25f},
    {"kk", "Jaw_Open", 0.10f},
    {"CH", "Mouth_Close", 0.20f},
    {"CH", "Jaw_Open", 0.25f},
    {"SS", "Mouth_Close", 0.20f},
    {"SS", "V_Wide", 0.15f},
    {"nn", "Mouth_Close", 0.35f},
    {"nn", "Jaw_Open", 0.10f},
    {"RR", "V_Tight", 0.50f},
    {"RR", "Mouth_Close", 0.15f},
    {"aa", "V_Open", 0.90f},
    {"aa", "Jaw_Open", 0.55f},
    {"E", "V_Wide", 0.60f},
    {"E", "Jaw_Open", 0.30f},
    {"I", "V_Wide", 0.80f},
    {"I", "Jaw_Open", 0.15f},
    {"O", "V_Tight_O", 0.90f},
    {"O", "Jaw_Open", 0.25f},
    {"U", "V_Tight_O", 0.70f},
    {"U", "Mouth_Close", 0.35f},
};

} // namespace

void VisemeTable::build(const QHash<QString, int> &nameToIndex) {
  m_table.clear();
  m_targetCount = nameToIndex.size();

  int applied = 0;
  int skipped = 0;

  for (const Spec &spec : kSpec) {
    const QString viseme = QString::fromLatin1(spec.viseme);
    const QString shape = QString::fromLatin1(spec.shape);

    const auto it = nameToIndex.constFind(shape);

    if (it == nameToIndex.constEnd()) {
      qWarning() << "[VisemeTable] Shape not present on the face mesh:"
                 << shape << "for viseme" << viseme;
      ++skipped;
      continue;
    }

    Weight w;
    w.morphIndex = it.value();
    w.weight = spec.weight;

    m_table[viseme].append(w);
    ++applied;
  }

  qDebug() << "[VisemeTable] Built:" << m_table.size() << "visemes,"
           << applied << "shape bindings," << skipped << "skipped,"
           << m_targetCount << "targets.";
}

QVector<VisemeTable::Weight> VisemeTable::weightsFor(
    const QString &viseme) const {
  return m_table.value(viseme);
}

QStringList VisemeTable::knownVisemes() const {
  QStringList names = m_table.keys();
  names.sort();
  return names;
}