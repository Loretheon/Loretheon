#include "../../include/avatar/VisemeTable.h"

#include <QDebug>

namespace {

constexpr float kDefaultPeak = 0.6f;
constexpr float kPeakPP = 0.9f;
constexpr float kPeakFF = 0.9f;

} // namespace

void VisemeTable::build(const QHash<QString, int> &nameToIndex) {
  m_table.clear();
  m_targetCount = nameToIndex.size();

  int applied = 0;
  int skipped = 0;

  QStringList missing;

  const QStringList visemes = {
      QStringLiteral("PP"),
      QStringLiteral("FF"),
      QStringLiteral("TH"),
      QStringLiteral("DD"),
      QStringLiteral("kk"),
      QStringLiteral("CH"),
      QStringLiteral("SS"),
      QStringLiteral("nn"),
      QStringLiteral("RR"),
      QStringLiteral("aa"),
      QStringLiteral("E"),
      QStringLiteral("I"),
      QStringLiteral("O"),
      QStringLiteral("U"),
      QStringLiteral("sil"),
  };

  for (const QString &viseme : visemes) {
    const QString shape = QStringLiteral("viseme_") + viseme;

    const auto it = nameToIndex.constFind(shape);

    if (it == nameToIndex.constEnd()) {
      missing.append(shape);
      ++skipped;
      continue;
    }

    float peak = kDefaultPeak;

    if (viseme == QStringLiteral("PP")) {
      peak = kPeakPP;
    } else if (viseme == QStringLiteral("FF")) {
      peak = kPeakFF;
    }

    Weight w;
    w.morphIndex = it.value();
    w.weight = peak;

    m_table[viseme].append(w);
    ++applied;
  }

  if (!missing.isEmpty()) {
    missing.removeDuplicates();
    missing.sort();

    qDebug() << "[VisemeTable] Visemes not present on the face mesh:"
             << missing.join(QStringLiteral(", "));
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