#pragma once

#include "../agent/Tool.h"

class OpenFileTool : public Tool {
public:
  QString name() const override { return QStringLiteral("open_file"); }

  QString description() const override;

  QString category() const override { return QStringLiteral("read"); }

  QJsonObject parametersSchema() const override;

  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};