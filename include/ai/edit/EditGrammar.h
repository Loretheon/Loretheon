#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace EditGrammar {

inline constexpr int kMaxEditsPerPlan = 8;

/*
 * Canonical edit-plan schema for OpenAI-compatible structured output.
 *
 * The schema is the sole constraint on the model's output. It is used
 * in both local and remote mode: llama.cpp's server converts a
 * json_schema response_format into a GBNF grammar internally, so the
 * same contract applies on both sides and there is only one code path
 * to keep correct.
 *
 * The top-level value is an object with a required "edits" array. This
 * is the shape OpenAI-compatible providers expect, and it is what
 * EditPlanner unwraps.
 */
inline QJsonObject jsonSchema(const QStringList &scopeIds) {
  QJsonObject operation;

  operation.insert(QStringLiteral("type"), QStringLiteral("string"));

  operation.insert(QStringLiteral("enum"),
                   QJsonArray{QStringLiteral("insert"),
                              QStringLiteral("replace"),
                              QStringLiteral("delete"),
                              QStringLiteral("replace_scope")});

  QJsonObject scope;

  scope.insert(QStringLiteral("type"), QStringLiteral("string"));

  QJsonArray scopeEnum;

  for (const QString &scopeId : scopeIds) {
    scopeEnum.append(scopeId);
  }

  scope.insert(QStringLiteral("enum"), scopeEnum);

  QJsonObject position;

  position.insert(QStringLiteral("type"), QStringLiteral("string"));

  position.insert(QStringLiteral("enum"), QJsonArray{QStringLiteral("before"),
                                                     QStringLiteral("after"),
                                                     QStringLiteral("inside")});

  QJsonObject find;

  find.insert(QStringLiteral("type"), QStringLiteral("string"));

  QJsonObject all;

  all.insert(QStringLiteral("type"), QStringLiteral("boolean"));

  QJsonObject instruction;

  instruction.insert(QStringLiteral("type"), QStringLiteral("string"));

  QJsonObject edit;

  QJsonObject editProperties;

  editProperties.insert(QStringLiteral("operation"), operation);

  editProperties.insert(QStringLiteral("scope"), scope);

  editProperties.insert(QStringLiteral("position"), position);

  editProperties.insert(QStringLiteral("find"), find);

  editProperties.insert(QStringLiteral("all"), all);

  editProperties.insert(QStringLiteral("instruction"), instruction);

  edit.insert(QStringLiteral("type"), QStringLiteral("object"));

  edit.insert(QStringLiteral("properties"), editProperties);

  edit.insert(QStringLiteral("required"),
              QJsonArray{QStringLiteral("operation"), QStringLiteral("scope"),
                         QStringLiteral("position"), QStringLiteral("find"),
                         QStringLiteral("all"), QStringLiteral("instruction")});

  edit.insert(QStringLiteral("additionalProperties"), false);

  QJsonObject edits;

  edits.insert(QStringLiteral("type"), QStringLiteral("array"));

  edits.insert(QStringLiteral("maxItems"), kMaxEditsPerPlan);

  edits.insert(QStringLiteral("items"), edit);

  QJsonObject root;

  QJsonObject rootProperties;

  rootProperties.insert(QStringLiteral("edits"), edits);

  root.insert(QStringLiteral("type"), QStringLiteral("object"));

  root.insert(QStringLiteral("properties"), rootProperties);

  root.insert(QStringLiteral("required"), QJsonArray{QStringLiteral("edits")});

  root.insert(QStringLiteral("additionalProperties"), false);

  return root;
}

inline QJsonObject jsonResponseFormat(const QStringList &scopeIds) {
  QJsonObject jsonSchemaDefinition;

  jsonSchemaDefinition.insert(QStringLiteral("name"),
                              QStringLiteral("edit_plan"));

  jsonSchemaDefinition.insert(QStringLiteral("strict"), true);

  jsonSchemaDefinition.insert(QStringLiteral("schema"), jsonSchema(scopeIds));

  QJsonObject responseFormat;

  responseFormat.insert(QStringLiteral("type"), QStringLiteral("json_schema"));

  responseFormat.insert(QStringLiteral("json_schema"), jsonSchemaDefinition);

  return responseFormat;
}

} // namespace EditGrammar