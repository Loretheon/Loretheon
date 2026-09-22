#pragma once

#include "../Tool.h"

class ListDirectoryTool : public Tool {
public:
  QString name() const override { return QStringLiteral("list_directory"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class ReadFileTool : public Tool {
public:
  QString name() const override { return QStringLiteral("read_file"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class ReadNotesFileTool : public Tool {
public:
  QString name() const override { return QStringLiteral("read_notes_file"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class WriteFileTool : public Tool {
public:
  QString name() const override { return QStringLiteral("write_file"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class CreateDirectoryTool : public Tool {
public:
  QString name() const override { return QStringLiteral("create_directory"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class OpenFileTool : public Tool {
public:
  QString name() const override { return QStringLiteral("open_file"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class CloseFileTool : public Tool {
public:
  QString name() const override { return QStringLiteral("close_file"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

// Memory proposal tools. The LLM chooses the scope by picking the tool:
// propose_global_memory_fact for facts that should persist across every
// session, propose_session_memory_fact for facts that belong to this
// session only.

class ProposeGlobalMemoryFactTool : public Tool {
public:
  QString name() const override {
    return QStringLiteral("propose_global_memory_fact");
  }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class ProposeSessionMemoryFactTool : public Tool {
public:
  QString name() const override {
    return QStringLiteral("propose_session_memory_fact");
  }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class Tools {
public:
  static void installAll(class ToolRegistry &registry);
};