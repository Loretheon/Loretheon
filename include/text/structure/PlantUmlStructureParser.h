#pragma once

#include "DocumentStructure.h"

#include <QString>

class PlantUmlStructureParser {
public:
  DocumentStructure parse(const QString &text) const;

private:
  static QString slugify(const QString &text);

  static QString makeStableId(const QString &kind, const QString &slug);
};