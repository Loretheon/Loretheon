#include "../../include/overseer/CloseFileTool.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

QString CloseFileTool::description() const {
  return QStringLiteral(
      "Close a file that is currently open in the Workstation. Relative "
      "paths only, relative to the output folder.");
}

QJsonObject CloseFileTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Relative path inside the output folder."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"), QJsonArray{QStringLiteral("path")});

  return schema;
}

Tool::Result
CloseFileTool::execute(const QJsonObject &arguments,
                       const Context &context) const {
  Result result;

  const QString relative = arguments.value(QStringLiteral("path")).toString();

  if (relative.isEmpty()) {
    result.ok = false;
    result.error = QStringLiteral("'path' is required.");
    return result;
  }

  const QString absolute =
      QDir(context.outputFolder).absoluteFilePath(relative);

  if (context.closeFile) {
    context.closeFile(absolute);
  }

  result.ok = true;
  result.output = QStringLiteral("Closed %1.").arg(relative);
  return result;
}