#pragma once

#include "Tool.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <memory>
#include <vector>

class ToolRegistry {
public:
  ToolRegistry();

  void registerTool(std::unique_ptr<Tool> tool);

  QJsonArray schemas() const;

  Tool::Result execute(const QString &name,
                               const QJsonObject &arguments,
                               const Tool::Context &context) const;

  QString categoryFor(const QString &name) const;

  bool isEmpty() const { return m_tools.empty(); }

private:
  Tool *find(const QString &name) const;

  std::vector<std::unique_ptr<Tool>> m_tools;
};