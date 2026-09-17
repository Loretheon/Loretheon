#pragma once

#include <QJsonObject>
#include <QString>

struct EditCommand {
  enum class Operation { Replace, Insert, Delete, ReplaceScope, Unknown };

  enum class Position { Before, After, Inside };

  Operation operation = Operation::Replace;
  Position position = Position::Inside;
  QString scopeId;
  QString findString;
  QString newString;
  QString instruction;
  bool replaceAll = false;

  // Alias helper for code calling command.scope
  QString scope() const { return scopeId; }

  // Target validation check (scope must be defined, operation-specific
  // constraints).
  bool isTargetValid() const {
    if (scopeId.isEmpty()) {
      return false;
    }

    if (operation == Operation::Insert) {
      return findString.isEmpty();
    }

    if (operation == Operation::ReplaceScope) {
      // Whole-scope replacement: no find string, position must be Inside.
      return findString.isEmpty() && position == Position::Inside;
    }

    if (operation == Operation::Replace || operation == Operation::Delete) {
      return !findString.isEmpty();
    }

    return false;
  }

  bool isCommandValid() const { return isTargetValid(); }

  bool isValid() const { return isTargetValid(); }

  static EditCommand fromJson(const QJsonObject &json) {
    EditCommand cmd;

    const QString opStr =
        json.value(QStringLiteral("operation")).toString().toLower();
    if (opStr == QStringLiteral("insert")) {
      cmd.operation = Operation::Insert;
    } else if (opStr == QStringLiteral("delete")) {
      cmd.operation = Operation::Delete;
    } else if (opStr == QStringLiteral("replace")) {
      cmd.operation = Operation::Replace;
    } else if (opStr == QStringLiteral("replace_scope")) {
      cmd.operation = Operation::ReplaceScope;
    } else {
      cmd.operation = Operation::Unknown;
    }

    const QString posStr =
        json.value(QStringLiteral("position")).toString().toLower();
    if (posStr == QStringLiteral("before")) {
      cmd.position = Position::Before;
    } else if (posStr == QStringLiteral("after")) {
      cmd.position = Position::After;
    } else {
      cmd.position = Position::Inside;
    }

    if (json.contains(QStringLiteral("scopeId"))) {
      cmd.scopeId = json.value(QStringLiteral("scopeId")).toString();
    } else {
      cmd.scopeId = json.value(QStringLiteral("scope")).toString();
    }

    if (json.contains(QStringLiteral("findString"))) {
      cmd.findString = json.value(QStringLiteral("findString")).toString();
    } else {
      cmd.findString = json.value(QStringLiteral("find")).toString();
    }

    if (json.contains(QStringLiteral("newString"))) {
      cmd.newString = json.value(QStringLiteral("newString")).toString();
    } else {
      cmd.newString = json.value(QStringLiteral("replace")).toString();
    }

    cmd.instruction = json.value(QStringLiteral("instruction")).toString();

    if (json.contains(QStringLiteral("replaceAll"))) {
      cmd.replaceAll = json.value(QStringLiteral("replaceAll")).toBool(false);
    } else {
      cmd.replaceAll = json.value(QStringLiteral("all")).toBool(false);
    }

    return cmd;
  }
};