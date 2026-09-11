#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace EditGrammar {

inline QString escapeLiteral(const QString &text) {
  QString escaped = text;

  escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));

  escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));

  return escaped;
}

inline QString makeAlternatives(const QStringList &values) {
  QStringList alternatives;

  alternatives.reserve(values.size());

  for (const QString &value : values) {

    alternatives.append(QStringLiteral("\"%1\"").arg(escapeLiteral(value)));
  }

  if (alternatives.isEmpty()) {

    return QStringLiteral("\"\"");
  }

  return alternatives.join(QStringLiteral(" | "));
}

/*
 * Bounded array of edit commands.
 *
 * This remains the local llama.cpp representation.
 *
 * Remote/OpenAI-compatible providers should use jsonSchema()
 * instead of this GBNF grammar.
 */
inline constexpr int kMaxEditsPerPlan = 8;

inline QString makeBoundedTailChain() {
  QString rules;

  for (int i = 0; i < kMaxEditsPerPlan - 1; ++i) {

    const int nextIndex = i + 1;

    if (nextIndex < kMaxEditsPerPlan - 1) {

      rules += QStringLiteral("commandTail%1 ::= "
                              "\"\" | ws \",\" ws command commandTail%2\n")
                   .arg(i)
                   .arg(nextIndex);

    } else {

      rules += QStringLiteral("commandTail%1 ::= \"\"\n").arg(i);
    }
  }

  return rules;
}

/*
 * Local llama.cpp grammar.
 *
 * This is deliberately retained for local inference.
 */
inline QString gbnf(const QStringList &scopeIds) {
  QString grammar = QStringLiteral("root ::= ws \"[\" ws editList ws \"]\" ws\n"

                                   "editList ::= "
                                   "\"\" | command commandTail0\n");

  grammar += makeBoundedTailChain();

  grammar += QStringLiteral("command ::= insert | replace | delete\n"

                            "insert ::= \"{\" ws "
                            "\"\\\"operation\\\"\" ws \":\" ws "
                            "\"\\\"insert\\\"\" ws \",\" ws "
                            "\"\\\"scope\\\"\" ws \":\" ws "
                            "\"\\\"\" scopeValue \"\\\"\" ws \",\" ws "
                            "\"\\\"position\\\"\" ws \":\" ws "
                            "\"\\\"\" positionValue \"\\\"\" ws \",\" ws "
                            "\"\\\"find\\\"\" ws \":\" ws "
                            "\"\\\"\\\"\" ws \",\" ws "
                            "\"\\\"all\\\"\" ws \":\" ws "
                            "boolean ws \",\" ws "
                            "\"\\\"instruction\\\"\" ws \":\" ws "
                            "string ws \"}\"\n"

                            "replace ::= \"{\" ws "
                            "\"\\\"operation\\\"\" ws \":\" ws "
                            "\"\\\"replace\\\"\" ws \",\" ws "
                            "\"\\\"scope\\\"\" ws \":\" ws "
                            "\"\\\"\" scopeValue \"\\\"\" ws \",\" ws "
                            "\"\\\"position\\\"\" ws \":\" ws "
                            "\"\\\"\" positionValue \"\\\"\" ws \",\" ws "
                            "\"\\\"find\\\"\" ws \":\" ws "
                            "string ws \",\" ws "
                            "\"\\\"all\\\"\" ws \":\" ws "
                            "boolean ws \",\" ws "
                            "\"\\\"instruction\\\"\" ws \":\" ws "
                            "string ws \"}\"\n"

                            "delete ::= \"{\" ws "
                            "\"\\\"operation\\\"\" ws \":\" ws "
                            "\"\\\"delete\\\"\" ws \",\" ws "
                            "\"\\\"scope\\\"\" ws \":\" ws "
                            "\"\\\"\" scopeValue \"\\\"\" ws \",\" ws "
                            "\"\\\"position\\\"\" ws \":\" ws "
                            "\"\\\"\" positionValue \"\\\"\" ws \",\" ws "
                            "\"\\\"find\\\"\" ws \":\" ws "
                            "string ws \",\" ws "
                            "\"\\\"all\\\"\" ws \":\" ws "
                            "boolean ws \",\" ws "
                            "\"\\\"instruction\\\"\" ws \":\" ws "
                            "string ws \"}\"\n"

                            "scopeValue ::= %1\n"

                            "positionValue ::= "
                            "\"before\" | "
                            "\"after\"\n"

                            "boolean ::= "
                            "\"true\" | "
                            "\"false\"\n"

                            "string ::= "
                            "\"\\\"\" char* \"\\\"\"\n"

                            "char ::= "
                            "[^\"\\\\\\x7F\\x00-\\x1F] | "
                            "\"\\\\\" "
                            "([\"\\\\/bfnrt] | "
                            "\"u\" [0-9a-fA-F]{4})\n"

                            "ws ::= [ \\t]*\n")
                 .arg(makeAlternatives(scopeIds));

  return grammar;
}

/*
 * Canonical edit-plan schema for OpenAI-compatible structured output.
 *
 * The root is an object rather than an array because OpenRouter's
 * structured-output interface is documented around JSON-schema objects.
 *
 * Example:
 *
 * {
 *   "edits": [
 *     {
 *       "operation": "replace",
 *       "scope": "paragraph-1",
 *       "position": "before",
 *       "find": "old text",
 *       "all": false,
 *       "instruction": "Replace this..."
 *     }
 *   ]
 * }
 *
 * scope is restricted to the actual scope IDs supplied by the caller.
 */
inline QJsonObject jsonSchema(const QStringList &scopeIds) {
  QJsonObject operation;

  operation.insert(QStringLiteral("type"), QStringLiteral("string"));

  operation.insert(QStringLiteral("enum"),
                   QJsonArray{QStringLiteral("insert"),
                              QStringLiteral("replace"),
                              QStringLiteral("delete")});

  QJsonObject scope;

  scope.insert(QStringLiteral("type"), QStringLiteral("string"));

  QJsonArray scopeEnum;

  for (const QString &scopeId : scopeIds) {

    scopeEnum.append(scopeId);
  }

  /*
   * If there are no scopes, an empty enum guarantees that no
   * edit can be produced. The caller should normally prevent
   * edit planning when scopeIds is empty.
   */
  scope.insert(QStringLiteral("enum"), scopeEnum);

  QJsonObject position;

  position.insert(QStringLiteral("type"), QStringLiteral("string"));

  position.insert(QStringLiteral("enum"), QJsonArray{QStringLiteral("before"),
                                                     QStringLiteral("after")});

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

/*
 * Complete OpenAI/OpenRouter response_format object.
 *
 * This can be inserted directly into the chat-completions request:
 *
 * "response_format": {
 *     "type": "json_schema",
 *     "json_schema": {
 *         "name": "edit_plan",
 *         "strict": true,
 *         "schema": { ... }
 *     }
 * }
 */
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