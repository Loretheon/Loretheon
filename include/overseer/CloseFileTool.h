#pragma once

#include "../agent/Tool.h"

class CloseFileTool : public Tool {
public:
  QString name() const override { return QStringLiteral("close_file"); }

  QString description() const override;

  QString category() const override { return QStringLiteral("read"); }

  QJsonObject parametersSchema() const override;

  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};