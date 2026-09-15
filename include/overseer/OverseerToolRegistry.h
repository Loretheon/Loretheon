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

  // Registers a tool. Ownership is transferred. Names must be unique; a
  // duplicate name replaces the existing tool.
  void registerTool(std::unique_ptr<OverseerTool> tool);

  // Returns the schemas for all registered tools, ready to be inserted into
  // a sendChatRequest call. Empty if no tools are registered.
  QJsonArray schemas() const;

  // Executes a tool by name. Returns an error result if the name is unknown
  // or the arguments are malformed.
  OverseerTool::Result execute(const QString &name,
                               const QJsonObject &arguments,
                               const OverseerTool::Context &context) const;

  bool isEmpty() const { return m_tools.empty(); }

private:
  OverseerTool *find(const QString &name) const;

  std::vector<std::unique_ptr<OverseerTool>> m_tools;
};