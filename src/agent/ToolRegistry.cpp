#include "../../include/agent/ToolRegistry.h"

ToolRegistry::ToolRegistry() = default;

void ToolRegistry::registerTool(std::unique_ptr<Tool> tool) {
  if (!tool)
    return;

  const QString toolName = tool->name();

  for (auto &existing : m_tools) {
    if (existing && existing->name() == toolName) {
      existing = std::move(tool);
      return;
    }
  }

  m_tools.push_back(std::move(tool));
}

QJsonArray ToolRegistry::schemas() const {
  QJsonArray result;

  for (const auto &tool : m_tools) {
    if (tool)
      result.append(tool->toSchema());
  }

  return result;
}

Tool *ToolRegistry::find(const QString &name) const {
  for (const auto &tool : m_tools) {
    if (tool && tool->name() == name)
      return tool.get();
  }
  return nullptr;
}

Tool::Result
ToolRegistry::execute(const QString &name,
                              const QJsonObject &arguments,
                              const Tool::Context &context) const {
  Tool *tool = find(name);

  if (!tool) {
    Tool::Result result;
    result.ok = false;
    result.error = QStringLiteral("Unknown tool '%1'.").arg(name);
    return result;
  }

  return tool->execute(arguments, context);
}

QString ToolRegistry::categoryFor(const QString &name) const {
  Tool *tool = find(name);
  return tool ? tool->category() : QStringLiteral("other");
}