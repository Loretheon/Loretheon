#include "../../include/avatar/VisemeTable.h"

#include <QDebug>

namespace {

struct Spec {
  const char *viseme;
  const char *shape;
  float weight;
};

constexpr Spec kSpec[] = {
    {"PP", "V_Explosive", 0.90f},
    {"PP", "Mouth_Close", 0.40f},

    {"FF", "V_Dental_Lip", 0.35f},
    {"FF", "Mouth_Close", 0.20f},
    {"FF", "Mouth_Up", 0.35f},
    {"FF", "Mouth_Lower_L", 0.18f},
    {"FF", "Mouth_Lower_R", 0.18f},

    {"TH", "V_Dental_Lip", 0.40f},
    {"TH", "Jaw_Open", 0.42f},
    {"TH", "Mouth_Up", 0.38f},
    {"TH", "Mouth_Drop_Lower", 0.25f},

    {"DD", "Mouth_Close", 0.60f},
    {"DD", "Jaw_Open", 0.40f},
    {"DD", "Mouth_Up", 0.22f},
    {"DD", "Mouth_Pull_Upper_L", 0.22f},
    {"DD", "Mouth_Pull_Upper_R", 0.22f},

    {"kk", "Mouth_Close", 0.55f},
    {"kk", "Jaw_Open", 0.35f},
    {"kk", "Mouth_Up", 0.18f},
    {"kk", "Jaw_Backward", 0.28f},

    {"CH", "V_Affricate", 0.85f},
    {"CH", "Jaw_Open", 0.50f},
    {"CH", "Mouth_Up", 0.28f},
    {"CH", "Mouth_Stretch_L", 0.25f},
    {"CH", "Mouth_Stretch_R", 0.25f},

    {"SS", "Mouth_Tighten_L", 0.50f},
    {"SS", "Mouth_Tighten_R", 0.50f},
    {"SS", "Mouth_Close", 0.30f},
    {"SS", "Jaw_Open", 0.15f},
    {"SS", "Mouth_Up", 0.20f},

    {"nn", "Mouth_Close", 0.62f},
    {"nn", "Jaw_Open", 0.28f},
    {"nn", "Mouth_Up", 0.18f},
    {"nn", "Mouth_Press_L", 0.30f},
    {"nn", "Mouth_Press_R", 0.30f},

    {"RR", "V_Tight", 0.60f},
    {"RR", "Mouth_Pucker_Up_L", 0.25f},
    {"RR", "Mouth_Pucker_Up_R", 0.25f},
    {"RR", "Jaw_Open", 0.32f},

    {"aa", "V_Open", 0.48f},
    {"aa", "Jaw_Open", 1.00f},
    {"aa", "Mouth_Up", 0.90f},
    {"aa", "Mouth_Drop_Lower", 0.72f},

    {"E", "V_Wide", 0.62f},
    {"E", "Jaw_Open", 0.85f},
    {"E", "Mouth_Up", 0.78f},
    {"E", "Mouth_Drop_Lower", 0.58f},
    {"E", "Mouth_Stretch_L", 0.35f},
    {"E", "Mouth_Stretch_R", 0.35f},
    {"E", "Mouth_Frown_L", 0.18f},
    {"E", "Mouth_Frown_R", 0.18f},

    {"I", "V_Wide", 0.88f},
    {"I", "Jaw_Open", 0.38f},
    {"I", "Mouth_Up", 0.30f},
    {"I", "Mouth_Stretch_L", 0.55f},
    {"I", "Mouth_Stretch_R", 0.55f},
    {"I", "Mouth_Smile_Sharp_L", 0.20f},
    {"I", "Mouth_Smile_Sharp_R", 0.20f},

    {"O", "V_Tight_O", 0.95f},
    {"O", "Jaw_Open", 0.68f},
    {"O", "Mouth_Up", 0.52f},
    {"O", "Mouth_Drop_Lower", 0.28f},
    {"O", "Mouth_Pucker_Up_L", 0.32f},
    {"O", "Mouth_Pucker_Up_R", 0.32f},
    {"O", "Mouth_Blow_L", 0.30f},
    {"O", "Mouth_Blow_R", 0.30f},

    {"U", "V_Tight_O", 0.85f},
    {"U", "Mouth_Close", 0.35f},
    {"U", "Mouth_Up", 0.28f},
    {"U", "Mouth_Pucker_Up_L", 0.55f},
    {"U", "Mouth_Pucker_Up_R", 0.55f},
    {"U", "Mouth_Press_L", 0.25f},
    {"U", "Mouth_Press_R", 0.25f},
    {"U", "Jaw_Open", 0.25f},
};

} // namespace

void VisemeTable::build(const QHash<QString, int> &nameToIndex) {
  m_table.clear();
  m_targetCount = nameToIndex.size();

  int applied = 0;
  int skipped = 0;

  QStringList missing;

  for (const Spec &spec : kSpec) {
    const QString viseme = QString::fromLatin1(spec.viseme);
    const QString shape = QString::fromLatin1(spec.shape);

    const auto it = nameToIndex.constFind(shape);

    if (it == nameToIndex.constEnd()) {
      missing.append(shape);
      ++skipped;
      continue;
    }

    Weight w;
    w.morphIndex = it.value();
    w.weight = spec.weight;

    m_table[viseme].append(w);
    ++applied;
  }

  if (!missing.isEmpty()) {
    missing.removeDuplicates();
    missing.sort();

    qDebug() << "[VisemeTable] Shapes not present on the face mesh:"
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