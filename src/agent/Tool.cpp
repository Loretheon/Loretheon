#include "../../include/agent/Tool.h"

QJsonObject Tool::toSchema() const {
  QJsonObject function;

  function.insert(QStringLiteral("name"), name());
  function.insert(QStringLiteral("description"), description());
  function.insert(QStringLiteral("parameters"), parametersSchema());

  QJsonObject tool;
  tool.insert(QStringLiteral("type"), QStringLiteral("function"));
  tool.insert(QStringLiteral("function"), function);

  return tool;
}