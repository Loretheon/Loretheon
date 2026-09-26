#include "../../include/overseer/OpenFileTool.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

QString OpenFileTool::description() const {
  return QStringLiteral(
      "Open a file from the session's output folder in the Workstation so "
      "the user can see it. Use this when you want to draw the user's "
      "attention to a specific file you are discussing. Relative paths "
      "only, relative to the output folder.");
}

QJsonObject OpenFileTool::parametersSchema() const {
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
OpenFileTool::execute(const QJsonObject &arguments,
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

  const QFileInfo info(absolute);

  if (!info.exists() || !info.isFile()) {
    result.ok = false;
    result.error = QStringLiteral("File does not exist: %1").arg(relative);
    return result;
  }

  const QString canonicalOutput =
      QDir(context.outputFolder).canonicalPath();
  const QString canonicalFile = info.canonicalFilePath();

  if (canonicalOutput.isEmpty() || canonicalFile.isEmpty() ||
      !canonicalFile.startsWith(canonicalOutput + QChar('/'))) {
    result.ok = false;
    result.error = QStringLiteral("Path is outside the output folder.");
    return result;
  }

  if (context.openFile) {
    context.openFile(canonicalFile);
  }

  result.ok = true;
  result.output = QStringLiteral("Opened %1 in the Workstation.").arg(relative);
  return result;
}