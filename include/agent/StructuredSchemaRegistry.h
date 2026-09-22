#pragma once

#include "StructuredSchema.h"

#include <QHash>
#include <QString>
#include <QStringList>

// Named collection of StructuredSchema values. Nothing more than a
// lookup table so that subsystems can refer to schemas by name.
class StructuredSchemaRegistry {
public:
  StructuredSchemaRegistry() = default;

  void add(const StructuredSchema &schema);

  // Returns a default-constructed (empty) schema if the name is not
  // registered. Use isEmpty() to check.
  StructuredSchema schemaFor(const QString &name) const;

  bool contains(const QString &name) const;

  QStringList names() const;

  // Register the schemas that ship with Lore. Currently:
  //   "sufficiency" — judge whether notes answer a question
  static void registerBuiltinSchemas(StructuredSchemaRegistry &registry);

private:
  QHash<QString, StructuredSchema> m_schemas;
};