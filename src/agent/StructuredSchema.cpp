#include "../../include/agent/StructuredSchema.h"

StructuredSchema::StructuredSchema(const QString &name,
                                   const QString &description)
    : m_name(name), m_description(description) {}

StructuredSchema &StructuredSchema::stringField(const QString &key,
                                                const QString &description,
                                                bool required) {
  addProperty(key, QStringLiteral("string"), description, required);
  return *this;
}

StructuredSchema &StructuredSchema::booleanField(const QString &key,
                                                 const QString &description,
                                                 bool required) {
  addProperty(key, QStringLiteral("boolean"), description, required);
  return *this;
}

StructuredSchema &StructuredSchema::integerField(const QString &key,
                                                 const QString &description,
                                                 bool required) {
  addProperty(key, QStringLiteral("integer"), description, required);
  return *this;
}

StructuredSchema &StructuredSchema::numberField(const QString &key,
                                                const QString &description,
                                                bool required) {
  addProperty(key, QStringLiteral("number"), description, required);
  return *this;
}

void StructuredSchema::addProperty(const QString &key, const QString &type,
                                   const QString &description,
                                   bool required) {
  QJsonObject property;
  property.insert(QStringLiteral("type"), type);

  if (!description.isEmpty()) {
    property.insert(QStringLiteral("description"), description);
  }

  m_properties.insert(key, property);

  if (required) {
    m_required.append(key);
  }
}

QJsonObject StructuredSchema::toSchemaObject() const {
  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), m_properties);
  schema.insert(QStringLiteral("required"), m_required);
  schema.insert(QStringLiteral("additionalProperties"), false);
  return schema;
}

QJsonObject StructuredSchema::toResponseFormat() const {
  QJsonObject jsonSchema;
  jsonSchema.insert(QStringLiteral("name"),
                    m_name.isEmpty() ? QStringLiteral("result") : m_name);
  jsonSchema.insert(QStringLiteral("strict"), true);
  jsonSchema.insert(QStringLiteral("schema"), toSchemaObject());

  QJsonObject responseFormat;
  responseFormat.insert(QStringLiteral("type"),
                        QStringLiteral("json_schema"));
  responseFormat.insert(QStringLiteral("json_schema"), jsonSchema);

  return responseFormat;
}