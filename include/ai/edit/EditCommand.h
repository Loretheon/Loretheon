#pragma once

#include <QString>

struct EditCommand {
  enum class Operation { Replace, Insert, Delete };

  enum class Position { Before, After };

  Operation operation = Operation::Replace;
  Position position = Position::Before;

  QString scopeId;
  QString findString;
  QString newString;

  bool replaceAll = false;

  bool isValid() const {
    switch (operation) {
    case Operation::Replace:
      return !scopeId.isEmpty() && !findString.isEmpty() &&
             !newString.isEmpty() && findString != newString;

    case Operation::Insert:
      return !scopeId.isEmpty() && !newString.isEmpty();

    case Operation::Delete:
      return !scopeId.isEmpty() && !findString.isEmpty();
    }

    return false;
  }
};