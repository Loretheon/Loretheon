#include "../../include/overseer/ProposeMemoryFactTool.h"

#include <QJsonArray>

QJsonObject ProposeMemoryFactTool::parametersSchema() const {
  QJsonObject fact;
  fact.insert(QStringLiteral("type"), QStringLiteral("string"));
  fact.insert(QStringLiteral("description"),
              QStringLiteral("A single sentence, no markdown, no bullet "
                             "prefix."));

  QJsonObject rationale;
  rationale.insert(QStringLiteral("type"), QStringLiteral("string"));
  rationale.insert(QStringLiteral("description"),
                   QStringLiteral("One short sentence explaining why this "
                                  "should persist."));

  QJsonObject properties;
  properties.insert(QStringLiteral("fact"), fact);
  properties.insert(QStringLiteral("rationale"), rationale);

  QJsonArray required;
  required.append(QStringLiteral("fact"));

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"), required);

  return schema;
}

OverseerTool::Result ProposeMemoryFactTool::execute(
    const QJsonObject &arguments, const Context &context) const {
  Q_UNUSED(context);

  Result result;

  const QString fact = arguments.value(QStringLiteral("fact")).toString().trimmed();

  if (fact.isEmpty()) {
    result.ok = false;
    result.error = QStringLiteral("Missing 'fact'.");
    return result;
  }

  result.ok = true;
  result.output = QStringLiteral(
      "Proposal recorded for user review. Continue only if you have more "
      "work to do; otherwise summarize and stop.");
  return result;
}