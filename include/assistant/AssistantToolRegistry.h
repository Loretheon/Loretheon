#pragma once

#include "AssistantTool.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <memory>
#include <vector>

namespace assistant {

class AssistantToolRegistry {
public:
  AssistantToolRegistry();

  void registerTool(std::unique_ptr<AssistantTool> tool);

  QJsonArray schemas() const;

  AssistantTool::Result execute(const QString &name,
                                const QJsonObject &arguments,
                                const AssistantToolContext &context) const;

  QString categoryFor(const QString &name) const;

  bool isDestructive(const QString &name) const;

  bool isEmpty() const { return m_tools.empty(); }

private:
  AssistantTool *find(const QString &name) const;

  std::vector<std::unique_ptr<AssistantTool>> m_tools;
};

} // namespace assistant