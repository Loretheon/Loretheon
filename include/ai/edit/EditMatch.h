#pragma once

struct EditMatch {
  int start = -1;
  int end = -1;
  int editDistance = -1;

  bool isValid() const { return start >= 0 && end > start; }
};