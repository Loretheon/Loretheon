#pragma once

#include "DocumentStructure.h"

class PlainTextStructureParser {
public:
  DocumentStructure parse(const QString &text) const;
};