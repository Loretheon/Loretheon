#include "../../include/agent/StructuredSchemaRegistry.h"

#include "../../include/agent/schemas/SufficiencySchema.h"

void StructuredSchemaRegistry::add(const StructuredSchema &schema) {
  if (schema.isEmpty()) {
    return;
  }

  m_schemas.insert(schema.name(), schema);
}

StructuredSchema
StructuredSchemaRegistry::schemaFor(const QString &name) const {
  return m_schemas.value(name);
}

bool StructuredSchemaRegistry::contains(const QString &name) const {
  return m_schemas.contains(name);
}

QStringList StructuredSchemaRegistry::names() const {
  QStringList result = m_schemas.keys();
  result.sort(Qt::CaseInsensitive);
  return result;
}

void StructuredSchemaRegistry::registerBuiltinSchemas(
    StructuredSchemaRegistry &registry) {
  registry.add(SufficiencySchema::schema());
}