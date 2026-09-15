#include "../../include/overseer/OverseerToolRegistry.h"

OverseerToolRegistry::OverseerToolRegistry() = default;

void OverseerToolRegistry::registerTool(std::unique_ptr<OverseerTool> tool) {
  if (!tool) {
    return;
  }

  const QString toolName = tool->name();

  for (auto &existing : m_tools) {
    if (existing && existing->name() == toolName) {
      existing = std::move(tool);
      return;
    }
  }

  m_tools.push_back(std::move(tool));
}

QJsonArray OverseerToolRegistry::schemas() const {
  QJsonArray result;

  for (const auto &tool : m_tools) {
    if (!tool) {
      continue;
    }

    result.append(tool->toSchema());
  }

  return result;
}

OverseerTool *OverseerToolRegistry::find(const QString &name) const {
  for (const auto &tool : m_tools) {
    if (tool && tool->name() == name) {
      return tool.get();
    }
  }

  return nullptr;
}

OverseerTool::Result
OverseerToolRegistry::execute(const QString &name,
                              const QJsonObject &arguments,
                              const OverseerTool::Context &context) const {
  OverseerTool *tool = find(name);

  if (!tool) {
    OverseerTool::Result result;
    result.ok = false;
    result.error = QStringLiteral("Unknown tool '%1'.").arg(name);
    return result;
  }

  return tool->execute(arguments, context);
}