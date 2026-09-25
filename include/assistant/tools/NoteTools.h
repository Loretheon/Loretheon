#pragma once

#include "../AssistantTool.h"

namespace assistant {
class AssistantToolRegistry;

class ListNotesTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("list_notes"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class ReadNoteTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("read_note"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class WriteNoteTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("write_note"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("write"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class EditNoteTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("edit_note"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("write"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class DeleteNoteTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("delete_note"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("write"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class NoteTools {
public:
  static void installAll(AssistantToolRegistry &registry);
};

} // namespace assistant