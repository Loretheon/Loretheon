#pragma once

#include <QString>

struct EditMatch {
  int start = -1;
  int end = -1;

  int editDistance = 0;

  QString matchedText;

  bool isValid() const { return start >= 0 && end >= start; }
};