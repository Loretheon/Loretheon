#pragma once

#include "OverseerTool.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <memory>
#include <vector>

class OverseerToolRegistry {
public:
  OverseerToolRegistry();

  void registerTool(std::unique_ptr<OverseerTool> tool);

  QJsonArray schemas() const;

  OverseerTool::Result execute(const QString &name,
                               const QJsonObject &arguments,
                               const OverseerTool::Context &context) const;

  QString categoryFor(const QString &name) const;

  bool isEmpty() const { return m_tools.empty(); }

private:
  OverseerTool *find(const QString &name) const;

  std::vector<std::unique_ptr<OverseerTool>> m_tools;
};