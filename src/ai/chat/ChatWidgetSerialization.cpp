#include "../../../include/ai/chat/ChatWidgetSerialization.h"

namespace ChatWidgetSerialization {

QString operationString(const EditCommand &command) {
  switch (command.operation) {
  case EditCommand::Operation::Insert:

    return QStringLiteral("insert");

  case EditCommand::Operation::Replace:

    return QStringLiteral("replace");

  case EditCommand::Operation::Delete:

    return QStringLiteral("delete");
  }

  return {};
}

QString positionString(const EditCommand &command) {
  switch (command.position) {
  case EditCommand::Position::Before:

    return QStringLiteral("before");

  case EditCommand::Position::After:

    return QStringLiteral("after");
  }

  return {};
}

QJsonObject editCommandToJson(const EditCommand &command) {
  QJsonObject object;

  object.insert(QStringLiteral("operation"), operationString(command));

  object.insert(QStringLiteral("scope"), command.scopeId);

  object.insert(QStringLiteral("position"), positionString(command));

  object.insert(QStringLiteral("find"), command.findString);

  object.insert(QStringLiteral("all"), command.replaceAll);

  object.insert(QStringLiteral("instruction"), command.instruction);

  return object;
}

} // namespace ChatWidgetSerialization