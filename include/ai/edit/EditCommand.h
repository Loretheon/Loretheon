#pragma once

#include <QString>

struct EditCommand {
  QString oldString;
  QString newString;

  bool isValid() const {
    return !oldString.isEmpty() && oldString != newString;
  }
};