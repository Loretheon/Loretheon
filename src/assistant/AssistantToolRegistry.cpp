#include "../../include/assistant/AssistantToolRegistry.h"

namespace assistant {

AssistantToolRegistry::AssistantToolRegistry() = default;

void AssistantToolRegistry::registerTool(
    std::unique_ptr<AssistantTool> tool) {
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

QJsonArray AssistantToolRegistry::schemas() const {
  QJsonArray result;

  for (const auto &tool : m_tools) {
    if (tool) {
      result.append(tool->toSchema());
    }
  }

  return result;
}

AssistantTool *AssistantToolRegistry::find(const QString &name) const {
  for (const auto &tool : m_tools) {
    if (tool && tool->name() == name) {
      return tool.get();
    }
  }
  return nullptr;
}

AssistantTool::Result AssistantToolRegistry::execute(
    const QString &name, const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  AssistantTool *tool = find(name);

  if (!tool) {
    AssistantTool::Result result;
    result.ok = false;
    result.error = QStringLiteral("Unknown tool '%1'.").arg(name);
    return result;
  }

  return tool->execute(arguments, context);
}

QString AssistantToolRegistry::categoryFor(const QString &name) const {
  AssistantTool *tool = find(name);
  return tool ? tool->category() : QStringLiteral("other");
}

bool AssistantToolRegistry::isDestructive(const QString &name) const {
  AssistantTool *tool = find(name);
  return tool ? tool->isDestructive() : false;
}

} // namespace assistant