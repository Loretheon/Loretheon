#pragma once

#include "../AssistantTool.h"

namespace assistant {
class AssistantToolRegistry;

// The tools Lore uses. She does not implement anything herself: she
// dispatches into the surfaces that already exist, and remembers
// facts when the user tells her something durable.
//
//   search        — runs a query through the user's notes and returns
//                   the synthesised answer. Runs on the assistant's
//                   own RetrievalLoop, independent of the Search page.
//
//   delegate      — hands a task to the Overseer conductor. The
//                   conductor runs it in a session and the runner
//                   reports back when it is done. The assistant does
//                   not wait.
//
//   remember_fact — writes a durable fact to the user profile, the
//                   self profile, or the memory tree.
//
//   speak         — speaks a line through the user's speakers.
class SearchTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("search"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class DelegateTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("delegate"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("write"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class RememberFactTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("remember_fact"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("memory"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class SpeakTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("speak"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("voice"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class PromoteNoteTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("promote_note"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("write"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class AssistantTools {
public:
  static void installAll(AssistantToolRegistry &registry);
};


} // namespace assistant