#include "../../include/overseer/OverseerTools.h"

#include "../../include/overseer/OverseerToolRegistry.h"
#include "EditNoteTool.h"
#include "EditWorkstationFileTool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QTextStream>

namespace {

QString safeResolve(const QString &relative, const QString &outputFolder) {
  if (relative.isEmpty() || relative == QStringLiteral(".") ||
      relative == QStringLiteral("./")) {
    return outputFolder;
  }

  if (QFileInfo(relative).isAbsolute()) {
    return {};
  }

  const QString cleaned = QDir::cleanPath(relative);

  if (cleaned == QStringLiteral(".") || cleaned.isEmpty()) {
    return outputFolder;
  }

  if (cleaned == QStringLiteral("..") ||
      cleaned.startsWith(QStringLiteral("../")) ||
      cleaned.contains(QStringLiteral("/../")) ||
      cleaned.endsWith(QStringLiteral("/.."))) {
    return {};
  }

  QDir output(outputFolder);

  const QString canonicalOutput = output.canonicalPath();

  if (canonicalOutput.isEmpty()) {
    return {};
  }

  const QString absolute = output.absoluteFilePath(cleaned);

  QString existingAncestor = QFileInfo(absolute).absolutePath();

  while (!existingAncestor.isEmpty() &&
         !QFileInfo::exists(existingAncestor) &&
         existingAncestor != outputFolder) {
    const QString parent = QFileInfo(existingAncestor).absolutePath();

    if (parent == existingAncestor) {
      return {};
    }

    existingAncestor = parent;
  }

  if (!QFileInfo::exists(existingAncestor)) {
    return {};
  }

  const QString canonicalAncestor =
      QDir(existingAncestor).canonicalPath();

  if (canonicalAncestor.isEmpty()) {
    return {};
  }

  if (canonicalAncestor != canonicalOutput &&
      !canonicalAncestor.startsWith(canonicalOutput + QChar('/'))) {
    return {};
  }

  return absolute;
}

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

QString describeRejection(const QString &relative) {
  return QStringLiteral(
             "Path rejected: \"%1\".\n"
             "Reason: paths passed to tools must be relative to the "
             "session output folder. They must not start with a slash "
             "(absolute paths are not allowed), must not start with a "
             "drive letter, and must not contain \"..\".\n"
             "Correct form: \"outline.md\" or \"manuscript/01-chapter-one.md\".\n"
             "Incorrect form: \"/home/user/notes/outline.md\", "
             "\"../outline.md\", \"output/outline.md\".")
      .arg(relative);
}

QJsonObject factRationaleSchema() {
  QJsonObject fact;
  fact.insert(QStringLiteral("type"), QStringLiteral("string"));
  fact.insert(QStringLiteral("description"),
              QStringLiteral("The durable fact, written as a single "
                             "sentence in the imperative or declarative."));

  QJsonObject rationale;
  rationale.insert(QStringLiteral("type"), QStringLiteral("string"));
  rationale.insert(QStringLiteral("description"),
                   QStringLiteral("One sentence explaining why this fact "
                                  "matters, for the user's benefit."));

  QJsonObject properties;
  properties.insert(QStringLiteral("fact"), fact);
  properties.insert(QStringLiteral("rationale"), rationale);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("fact"), QStringLiteral("rationale")});

  return schema;
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
    return makeError(describeRejection(relative));
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
    return makeError(describeRejection(relative));
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
    return makeError(
        QStringLiteral("Path rejected: \"%1\".\n"
                       "Reason: the file must exist and be inside the "
                       "notes root. Pass either a notes-root-relative path "
                       "(e.g. \"foo.md\") or an absolute path inside the "
                       "notes root.")
            .arg(requested));
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
    return makeError(describeRejection(relative));
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
    return makeError(describeRejection(relative));
  }

  if (!QDir().mkpath(absolute)) {
    return makeError(QStringLiteral("Could not create directory."));
  }

  return makeOk(QStringLiteral("Created directory %1").arg(relative));
}

QString ProposeGlobalMemoryFactTool::description() const {
  return QStringLiteral(
      "Propose a durable fact that should persist across every session. "
      "Use this for user preferences, standing rules, and anything the "
      "assistant should always remember, regardless of which session it "
      "is in. The user accepts or rejects the proposal before it is "
      "written to global memory.");
}

QJsonObject ProposeGlobalMemoryFactTool::parametersSchema() const {
  return factRationaleSchema();
}

OverseerTool::Result
ProposeGlobalMemoryFactTool::execute(const QJsonObject &arguments,
                                     const Context &context) const {
  Q_UNUSED(context);

  const QString fact =
      arguments.value(QStringLiteral("fact")).toString().trimmed();

  if (fact.isEmpty()) {
    return makeError(QStringLiteral("Proposed fact is empty."));
  }

  return makeOk(QStringLiteral("Proposed global memory fact."));
}

QString ProposeSessionMemoryFactTool::description() const {
  return QStringLiteral(
      "Propose a fact scoped to this session only. Use this for details "
      "that matter for the current body of work but should not become "
      "standing rules: local file names, this session's terminology, "
      "decisions made here. The user accepts or rejects the proposal "
      "before it is written to session memory.");
}

QJsonObject ProposeSessionMemoryFactTool::parametersSchema() const {
  return factRationaleSchema();
}

OverseerTool::Result
ProposeSessionMemoryFactTool::execute(const QJsonObject &arguments,
                                      const Context &context) const {
  Q_UNUSED(context);

  const QString fact =
      arguments.value(QStringLiteral("fact")).toString().trimmed();

  if (fact.isEmpty()) {
    return makeError(QStringLiteral("Proposed fact is empty."));
  }

  return makeOk(QStringLiteral("Proposed session memory fact."));
}

void OverseerTools::installAll(OverseerToolRegistry &registry) {
  registry.registerTool(std::make_unique<ListDirectoryTool>());
  registry.registerTool(std::make_unique<ReadFileTool>());
  registry.registerTool(std::make_unique<ReadNotesFileTool>());
  registry.registerTool(std::make_unique<WriteFileTool>());
  registry.registerTool(std::make_unique<CreateDirectoryTool>());
  registry.registerTool(std::make_unique<OpenFileTool>());
  registry.registerTool(std::make_unique<CloseFileTool>());
  registry.registerTool(std::make_unique<EditWorkstationFileTool>());
  registry.registerTool(std::make_unique<ProposeGlobalMemoryFactTool>());
  registry.registerTool(std::make_unique<ProposeSessionMemoryFactTool>());
  registry.registerTool(std::make_unique<EditNoteTool>());
}