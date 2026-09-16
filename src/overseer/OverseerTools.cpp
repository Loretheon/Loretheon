#include "../../include/overseer/OverseerTools.h"

#include "../../include/overseer/OverseerToolRegistry.h"
#include "ProposeMemoryFactTool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QTextStream>

namespace {

QString safeResolve(const QString &relative, const QString &outputFolder) {
  // Empty, ".", and "./" all refer to the output folder itself. Earlier
  // versions only handled the empty case, which caused the model's
  // "." and "./" arguments to be rejected.
  if (relative.isEmpty() || relative == QStringLiteral(".") ||
      relative == QStringLiteral("./")) {
    return outputFolder;
  }

  QDir output(outputFolder);

  if (QFileInfo(relative).isAbsolute()) {
    return {};
  }

  QString cleaned = QDir::cleanPath(relative);

  if (cleaned == QStringLiteral(".") || cleaned.isEmpty()) {
    return outputFolder;
  }

  if (cleaned.startsWith(QStringLiteral("..")) ||
      cleaned.contains(QStringLiteral("/../")) ||
      cleaned.endsWith(QStringLiteral("/.."))) {
    return {};
  }

  const QString absolute = output.absoluteFilePath(cleaned);

  const QString canonicalOutput = output.canonicalPath();

  QFileInfo info(absolute);

  QString canonicalParent;

  if (info.exists()) {
    canonicalParent = info.canonicalPath();
  } else {
    QDir parent(info.absolutePath());
    canonicalParent = parent.canonicalPath();
  }

  if (canonicalOutput.isEmpty() || canonicalParent.isEmpty()) {
    return {};
  }

  if (canonicalParent != canonicalOutput &&
      !canonicalParent.startsWith(canonicalOutput + QChar('/'))) {
    return {};
  }

  return absolute;
}

// Resolves a model-supplied path against the notes root. Unlike
// safeResolve, this does not restrict to a subfolder, but it does
// reject anything that escapes the notes root via symlinks or .. .
QString safeResolveNotes(const QString &path, const QString &notesRoot) {
  if (path.isEmpty()) {
    return {};
  }

  QFileInfo info(path);

  QString absolute;

  if (info.isAbsolute()) {
    absolute = QDir::cleanPath(path);
  } else {
    absolute = QDir(notesRoot).absoluteFilePath(path);
  }

  if (!QFileInfo::exists(absolute)) {
    return {};
  }

  const QString canonicalRoot = QDir(notesRoot).canonicalPath();

  const QFileInfo canonical(absolute);

  const QString canonicalPath = canonical.canonicalFilePath();

  if (canonicalRoot.isEmpty() || canonicalPath.isEmpty()) {
    return {};
  }

  if (canonicalPath != canonicalRoot &&
      !canonicalPath.startsWith(canonicalRoot + QChar('/'))) {
    return {};
  }

  return canonicalPath;
}

OverseerTool::Result makeError(const QString &message) {
  OverseerTool::Result result;
  result.ok = false;
  result.error = message;
  return result;
}

OverseerTool::Result makeOk(const QString &output) {
  OverseerTool::Result result;
  result.ok = true;
  result.output = output;
  return result;
}

} // namespace

QString ListDirectoryTool::description() const {
  return QStringLiteral(
      "List the contents of a directory inside the session's output folder. "
      "Pass \".\" or \"\" to list the output folder itself.");
}

QJsonObject ListDirectoryTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Relative path inside the output folder. "
                             "Use \".\" or an empty string for the output "
                             "folder itself."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"), QJsonArray{QStringLiteral("path")});

  return schema;
}

OverseerTool::Result
ListDirectoryTool::execute(const QJsonObject &arguments,
                           const Context &context) const {
  const QString relative = arguments.value(QStringLiteral("path")).toString();

  const QString absolute = safeResolve(relative, context.outputFolder);

  if (absolute.isEmpty()) {
    return makeError(QStringLiteral(
        "Path is outside the output folder or invalid."));
  }

  QFileInfo info(absolute);

  if (!info.exists() || !info.isDir()) {
    return makeError(
        QStringLiteral("Directory does not exist: %1")
            .arg(relative.isEmpty() ? QStringLiteral(".") : relative));
  }

  QDir dir(absolute);

  const QFileInfoList entries = dir.entryInfoList(
      QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

  if (entries.isEmpty()) {
    return makeOk(QStringLiteral("(empty directory)"));
  }

  QStringList lines;

  for (const QFileInfo &entry : entries) {
    const QString kind = entry.isDir() ? QStringLiteral("dir ")
                                        : QStringLiteral("file");
    const qint64 size = entry.isDir() ? 0 : entry.size();
    lines.append(QStringLiteral("%1  %2  %3 bytes")
                     .arg(kind, entry.fileName())
                     .arg(size));
  }

  return makeOk(lines.join(QChar('\n')));
}

QString ReadFileTool::description() const {
  return QStringLiteral(
      "Read a UTF-8 text file from the session's output folder. "
      "Refuses files outside the output folder. For files under the notes "
      "root that the user has referenced, use read_notes_file instead.");
}

QJsonObject ReadFileTool::parametersSchema() const {
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

OverseerTool::Result
ReadFileTool::execute(const QJsonObject &arguments,
                      const Context &context) const {
  const QString relative = arguments.value(QStringLiteral("path")).toString();

  const QString absolute = safeResolve(relative, context.outputFolder);

  if (absolute.isEmpty()) {
    return makeError(QStringLiteral(
        "Path is outside the output folder or invalid."));
  }

  QFileInfo info(absolute);

  if (!info.exists() || !info.isFile()) {
    return makeError(QStringLiteral("File does not exist: %1").arg(relative));
  }

  if (info.size() > 1024 * 1024) {
    return makeError(QStringLiteral("File is larger than 1 MiB."));
  }

  QFile file(absolute);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return makeError(
        QStringLiteral("Could not open file: %1").arg(file.errorString()));
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString content = stream.readAll();

  if (stream.status() != QTextStream::Ok) {
    return makeError(
        QStringLiteral("Could not read file: %1").arg(file.errorString()));
  }

  return makeOk(content);
}

QString ReadNotesFileTool::description() const {
  return QStringLiteral(
      "Read a UTF-8 text file from the user's notes root. Accepts either "
      "an absolute path inside the notes root, or a path relative to it. "
      "Read-only: this tool cannot modify anything. Use this for files the "
      "user has referenced in the Overview.");
}

QJsonObject ReadNotesFileTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Absolute or notes-root-relative path to a "
                             "file inside the notes root."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"), QJsonArray{QStringLiteral("path")});

  return schema;
}

OverseerTool::Result
ReadNotesFileTool::execute(const QJsonObject &arguments,
                           const Context &context) const {
  const QString requested = arguments.value(QStringLiteral("path")).toString();

  if (context.notesRoot.isEmpty()) {
    return makeError(QStringLiteral("Notes root is not configured."));
  }

  const QString absolute = safeResolveNotes(requested, context.notesRoot);

  if (absolute.isEmpty()) {
    return makeError(QStringLiteral(
        "Path is outside the notes root or does not exist."));
  }

  QFileInfo info(absolute);

  if (!info.isFile()) {
    return makeError(QStringLiteral("Not a file: %1").arg(requested));
  }

  if (info.size() > 2 * 1024 * 1024) {
    return makeError(QStringLiteral("File is larger than 2 MiB."));
  }

  QFile file(absolute);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return makeError(
        QStringLiteral("Could not open file: %1").arg(file.errorString()));
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString content = stream.readAll();

  if (stream.status() != QTextStream::Ok) {
    return makeError(
        QStringLiteral("Could not read file: %1").arg(file.errorString()));
  }

  return makeOk(content);
}

QString WriteFileTool::description() const {
  return QStringLiteral(
      "Create or overwrite a UTF-8 text file inside the session's output "
      "folder. Parent directories are created automatically. Cannot write "
      "outside the output folder.");
}

QJsonObject WriteFileTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Relative path inside the output folder."));

  QJsonObject content;
  content.insert(QStringLiteral("type"), QStringLiteral("string"));
  content.insert(QStringLiteral("description"),
                 QStringLiteral("The full content of the file."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);
  properties.insert(QStringLiteral("content"), content);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("path"), QStringLiteral("content")});

  return schema;
}

OverseerTool::Result
WriteFileTool::execute(const QJsonObject &arguments,
                       const Context &context) const {
  const QString relative = arguments.value(QStringLiteral("path")).toString();

  const QString content = arguments.value(QStringLiteral("content")).toString();

  const QString absolute = safeResolve(relative, context.outputFolder);

  if (absolute.isEmpty()) {
    return makeError(QStringLiteral(
        "Path is outside the output folder or invalid."));
  }

  QFileInfo info(absolute);

  QDir parent(info.absolutePath());

  if (!parent.exists()) {
    if (!QDir().mkpath(parent.absolutePath())) {
      return makeError(QStringLiteral("Could not create parent directory."));
    }
  }

  QFile file(absolute);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text)) {
    return makeError(
        QStringLiteral("Could not open file for writing: %1")
            .arg(file.errorString()));
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << content;

  if (stream.status() != QTextStream::Ok) {
    return makeError(
        QStringLiteral("Could not write file: %1").arg(file.errorString()));
  }

  return makeOk(QStringLiteral("Wrote %1 bytes to %2")
                    .arg(content.toUtf8().size())
                    .arg(relative));
}

QString CreateDirectoryTool::description() const {
  return QStringLiteral(
      "Create a directory inside the session's output folder. "
      "Parent directories are created automatically.");
}

QJsonObject CreateDirectoryTool::parametersSchema() const {
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

OverseerTool::Result
CreateDirectoryTool::execute(const QJsonObject &arguments,
                             const Context &context) const {
  const QString relative = arguments.value(QStringLiteral("path")).toString();

  const QString absolute = safeResolve(relative, context.outputFolder);

  if (absolute.isEmpty()) {
    return makeError(QStringLiteral(
        "Path is outside the output folder or invalid."));
  }

  if (!QDir().mkpath(absolute)) {
    return makeError(QStringLiteral("Could not create directory."));
  }

  return makeOk(QStringLiteral("Created directory %1").arg(relative));
}

void OverseerTools::installAll(OverseerToolRegistry &registry) {
  registry.registerTool(std::make_unique<ListDirectoryTool>());
  registry.registerTool(std::make_unique<ReadFileTool>());
  registry.registerTool(std::make_unique<ReadNotesFileTool>());
  registry.registerTool(std::make_unique<WriteFileTool>());
  registry.registerTool(std::make_unique<CreateDirectoryTool>());
  registry.registerTool(std::make_unique<ProposeMemoryFactTool>());
}