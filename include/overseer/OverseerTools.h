#pragma once

#include "OverseerTool.h"

class ListDirectoryTool : public OverseerTool {
public:
  QString name() const override { return QStringLiteral("list_directory"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class ReadFileTool : public OverseerTool {
public:
  QString name() const override { return QStringLiteral("read_file"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class ReadNotesFileTool : public OverseerTool {
public:
  QString name() const override { return QStringLiteral("read_notes_file"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class WriteFileTool : public OverseerTool {
public:
  QString name() const override { return QStringLiteral("write_file"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class CreateDirectoryTool : public OverseerTool {
public:
  QString name() const override { return QStringLiteral("create_directory"); }
  QString description() const override;
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class OpenFileTool : public OverseerTool {
public:
  QString name() const override { return QStringLiteral("open_file"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class CloseFileTool : public OverseerTool {
public:
  QString name() const override { return QStringLiteral("close_file"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};

class OverseerTools {
public:
  static void installAll(class OverseerToolRegistry &registry);
};