#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// A named JSON schema for structured output. Wraps the fields the
// OpenAI-compatible response_format expects and produces the exact
// object to pass to InferenceService::sendChatRequest.
//
// The schema is a value type. Copy it, store it, register it. It
// carries no runtime state.
//
// Field types supported: "string", "boolean", "integer", "number".
// Arrays and nested objects are not exposed through the builder —
// define them by hand if you need them.
class StructuredSchema {
public:
  StructuredSchema() = default;

  StructuredSchema(const QString &name, const QString &description);

  // Fluent property builders. Each returns *this so calls can chain.
  StructuredSchema &stringField(const QString &key,
                                const QString &description,
                                bool required = true);

  StructuredSchema &booleanField(const QString &key,
                                 const QString &description,
                                 bool required = true);

  StructuredSchema &integerField(const QString &key,
                                 const QString &description,
                                 bool required = true);

  StructuredSchema &numberField(const QString &key,
                                const QString &description,
                                bool required = true);

  QString name() const { return m_name; }
  QString description() const { return m_description; }

  bool isEmpty() const { return m_name.isEmpty(); }

  // The value to pass as the responseFormat argument to
  // InferenceService::sendChatRequest. Shape:
  //
  //   {"type": "json_schema",
  //    "json_schema": {"name": ..., "strict": true, "schema": {...}}}
  QJsonObject toResponseFormat() const;

  // The bare schema object, without the response_format wrapper. Used
  // when the caller wants to embed it in a larger prompt.
  QJsonObject toSchemaObject() const;

private:
  void addProperty(const QString &key, const QString &type,
                   const QString &description, bool required);

  QString m_name;
  QString m_description;

  QJsonObject m_properties;
  QJsonArray m_required;
};