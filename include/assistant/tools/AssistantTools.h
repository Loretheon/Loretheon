#pragma once

#include "../AssistantTool.h"

namespace assistant {
class AssistantToolRegistry;

// The concrete tools the assistant can call. Each is small and does
// one thing. Registration is explicit in installAll().

class SearchNotesTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("search_notes"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  bool isDestructive() const override { return false; }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class OpenFileTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("open_file"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  bool isDestructive() const override { return false; }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class InsertTextTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("insert_text"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("write"); }
  bool isDestructive() const override { return true; }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class RememberFactTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("remember_fact"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("memory"); }
  bool isDestructive() const override { return false; }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class ChangeSettingTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("change_setting"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("settings"); }
  bool isDestructive() const override { return true; }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class SpeakTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("speak"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("voice"); }
  bool isDestructive() const override { return false; }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class AssistantTools {
public:
  static void installAll(AssistantToolRegistry &registry);
};

} // namespace assistant