#pragma once

#include "../agent/Tool.h"

class EditWorkstationFileTool : public Tool {
public:
  QString name() const override {
    return QStringLiteral("edit_workstation_file");
  }

  QString description() const override;

  QString category() const override { return QStringLiteral("edit"); }

  QJsonObject parametersSchema() const override;

  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};