#include "../../../include/assistant/tools/NoteTools.h"

#include "../../../include/ai/edit/EditPlanner.h"
#include "../../../include/app/DocumentManager.h"
#include "../../../include/assistant/AssistantToolRegistry.h"
#include "../../../include/search/ScopeIndex.h"
#include "../../../include/text/TextEdit.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QStandardPaths>

namespace assistant {

namespace {

AssistantTool::Result makeError(const QString &message) {
  AssistantTool::Result result;
  result.ok = false;
  result.error = message;
  return result;
}

AssistantTool::Result makeOk(const QString &output) {
  AssistantTool::Result result;
  result.ok = true;
  result.output = output;
  return result;
}

QString notesRootFor(const AssistantToolContext &context) {
  if (!context.notesRoot.isEmpty()) {
    return context.notesRoot;
  }

  return QStandardPaths::writableLocation(
             QStandardPaths::AppDataLocation) +
         QStringLiteral("/notes");
}

bool isInsideNotes(const QString &absolutePath, const QString &notesRoot) {
  const QString normalizedRoot =
      QDir(notesRoot).absolutePath() + QChar('/');
  const QString normalizedPath = QDir(absolutePath).absolutePath();

  return normalizedPath.startsWith(normalizedRoot);
}

QString resolveNotePath(const QString &relativeOrAbsolute,
                        const QString &notesRoot,
                        QString *errorOut) {
  if (relativeOrAbsolute.isEmpty()) {
    if (errorOut)
      *errorOut = QStringLiteral("No path given.");
    return {};
  }

  QString absolute;

  if (QDir::isAbsolutePath(relativeOrAbsolute)) {
    absolute = relativeOrAbsolute;
  } else {
    absolute = QDir(notesRoot).absoluteFilePath(relativeOrAbsolute);
  }

  absolute = QDir::cleanPath(absolute);

  if (!isInsideNotes(absolute, notesRoot)) {
    if (errorOut) {
      *errorOut = QStringLiteral(
          "Path is outside the notes folder: %1").arg(absolute);
    }
    return {};
  }

  if (errorOut)
    errorOut->clear();

  return absolute;
}

QString trashRoot() {
  return QStandardPaths::writableLocation(
             QStandardPaths::AppDataLocation) +
         QStringLiteral("/trash");
}

} // namespace

// ---------------------------------------------------------------------
// ListNotesTool
// ---------------------------------------------------------------------

QString ListNotesTool::description() const {
  return QStringLiteral(
      "List every note in the user's notes folder, with a one-line "
      "preview of each. Use this to see what exists before reading or "
      "writing, or when the user asks what is in their notes.");
}

QJsonObject ListNotesTool::parametersSchema() const {
  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), QJsonObject());
  return schema;
}

AssistantTool::Result ListNotesTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  Q_UNUSED(arguments);

  const QString notesRoot = notesRootFor(context);

  QDir root(notesRoot);

  if (!root.exists()) {
    return makeOk(QStringLiteral("The notes folder is empty."));
  }

  QStringList files;

  QDirIterator it(notesRoot, QStringList{QStringLiteral("*.md")},
                  QDir::Files | QDir::Readable,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    files.append(it.next());
  }

  if (files.isEmpty()) {
    return makeOk(QStringLiteral("The notes folder is empty."));
  }

  files.sort(Qt::CaseInsensitive);

  QString listing;

  for (const QString &file : std::as_const(files)) {
    const QString relative = root.relativeFilePath(file);

    QString preview;

    if (context.scopeIndex) {
      preview = context.scopeIndex->previewFor(file);
    }

    listing += QStringLiteral("- %1").arg(relative);

    if (!preview.isEmpty()) {
      listing += QStringLiteral(" — %1").arg(preview);
    }

    listing += QChar('\n');
  }

  return makeOk(listing);
}

// ---------------------------------------------------------------------
// ReadNoteTool
// ---------------------------------------------------------------------

QString ReadNoteTool::description() const {
  return QStringLiteral(
      "Read the full contents of a note. Use the path returned by "
      "list_notes or search. Paths are relative to the notes folder.");
}

QJsonObject ReadNoteTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Path to the note, relative to the "
                             "notes folder."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("path")});

  return schema;
}

AssistantTool::Result ReadNoteTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  const QString notesRoot = notesRootFor(context);

  QString error;
  const QString absolute = resolveNotePath(
      arguments.value(QStringLiteral("path")).toString(), notesRoot,
      &error);

  if (absolute.isEmpty()) {
    return makeError(error);
  }

  QFile file(absolute);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return makeError(
        QStringLiteral("Could not read %1").arg(absolute));
  }

  const QString body = QString::fromUtf8(file.readAll());
  file.close();

  if (body.isEmpty()) {
    return makeOk(QStringLiteral("(the note is empty)"));
  }

  return makeOk(body);
}

// ---------------------------------------------------------------------
// WriteNoteTool
// ---------------------------------------------------------------------

QString WriteNoteTool::description() const {
  return QStringLiteral(
      "Create a new note in the user's notes folder. Use this when "
      "the note is small and self-contained — a single topic, a "
      "shopping list, a summary, a single page. Do not use it for "
      "material that spans many files or needs planning; use delegate "
      "for that.\n"
      "\n"
      "The path must not already exist. This tool never overwrites an "
      "existing note. To change a note that already exists, use "
      "edit_note instead.");
}

QJsonObject WriteNoteTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Path to the note, relative to the notes "
                             "folder. May contain subfolders. Must end "
                             "in .md."));

  QJsonObject content;
  content.insert(QStringLiteral("type"), QStringLiteral("string"));
  content.insert(QStringLiteral("description"),
                 QStringLiteral("The full Markdown body of the note."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);
  properties.insert(QStringLiteral("content"), content);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("path"),
                           QStringLiteral("content")});

  return schema;
}

AssistantTool::Result WriteNoteTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  const QString notesRoot = notesRootFor(context);

  QString error;
  const QString absolute = resolveNotePath(
      arguments.value(QStringLiteral("path")).toString(), notesRoot,
      &error);

  if (absolute.isEmpty()) {
    return makeError(error);
  }

  if (!absolute.endsWith(QStringLiteral(".md"))) {
    return makeError(QStringLiteral("Note paths must end in .md"));
  }

  if (QFileInfo::exists(absolute)) {
    return makeError(QStringLiteral(
        "A note already exists at %1. Use edit_note to change it.")
                         .arg(absolute));
  }

  const QString content =
      arguments.value(QStringLiteral("content")).toString();

  if (content.trimmed().isEmpty()) {
    return makeError(QStringLiteral("The note body is empty."));
  }

  const QFileInfo info(absolute);
  const QDir parent = info.absoluteDir();

  if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
    return makeError(
        QStringLiteral("Could not create the note's folder."));
  }

  QFile file(absolute);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return makeError(
        QStringLiteral("Could not write the note to disk."));
  }

  file.write(content.toUtf8());
  file.close();

  if (context.scopeIndex) {
    context.scopeIndex->addFile(absolute);
  }

  const QString relative = QDir(notesRoot).relativeFilePath(absolute);

  return makeOk(QStringLiteral("Wrote note %1.").arg(relative));
}

// ---------------------------------------------------------------------
// EditNoteTool
// ---------------------------------------------------------------------

QString EditNoteTool::description() const {
  return QStringLiteral(
      "Change an existing note. Provide the path and a plain-English "
      "instruction describing the change. The note is opened in the "
      "editor and the standard edit pipeline plans and applies the "
      "change autonomously, without review. Returns a job id "
      "immediately. Use read_job to read the result when the edit "
      "completes.");
}

QJsonObject EditNoteTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Path to the note, relative to the notes "
                             "folder."));

  QJsonObject instruction;
  instruction.insert(QStringLiteral("type"), QStringLiteral("string"));
  instruction.insert(
      QStringLiteral("description"),
      QStringLiteral("A plain-English description of the change, "
                     "e.g. 'add a section about X under the heading "
                     "Y' or 'replace the paragraph about Z with…'."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);
  properties.insert(QStringLiteral("instruction"), instruction);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("path"),
                           QStringLiteral("instruction")});

  return schema;
}

AssistantTool::Result EditNoteTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.documents) {
    return makeError(QStringLiteral("No document manager available."));
  }

  const QString notesRoot = notesRootFor(context);

  QString error;
  const QString absolute = resolveNotePath(
      arguments.value(QStringLiteral("path")).toString(), notesRoot,
      &error);

  if (absolute.isEmpty()) {
    return makeError(error);
  }

  if (!QFileInfo::exists(absolute)) {
    return makeError(
        QStringLiteral("No note exists at %1. Use write_note to "
                       "create it.").arg(absolute));
  }

  const QString instruction =
      arguments.value(QStringLiteral("instruction")).toString().trimmed();

  if (instruction.isEmpty()) {
    return makeError(QStringLiteral("The instruction is empty."));
  }

  // The conductor intercepts this marker, starts the edit job, and
  // returns the job id to the model. Nothing blocks. The path and
  // instruction are joined with a newline, because a path cannot
  // contain a newline.
  return makeOk(QStringLiteral("__job_edit__:") + absolute +
                QChar('\n') + instruction);
}

// ---------------------------------------------------------------------
// DeleteNoteTool
// ---------------------------------------------------------------------

QString DeleteNoteTool::description() const {
  return QStringLiteral(
      "Move a note to the trash. The note is not permanently deleted; "
      "it can be recovered from the trash folder. Use only when the "
      "user has asked for the note to be removed.");
}

QJsonObject DeleteNoteTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Path to the note, relative to the notes "
                             "folder."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("path")});

  return schema;
}

AssistantTool::Result DeleteNoteTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  const QString notesRoot = notesRootFor(context);

  QString error;
  const QString absolute = resolveNotePath(
      arguments.value(QStringLiteral("path")).toString(), notesRoot,
      &error);

  if (absolute.isEmpty()) {
    return makeError(error);
  }

  if (!QFileInfo::exists(absolute)) {
    return makeError(QStringLiteral("No note exists at %1").arg(absolute));
  }

  const QString trash = trashRoot();

  if (!QDir().mkpath(trash)) {
    return makeError(
        QStringLiteral("Could not create the trash folder."));
  }

  const QString stamp =
      QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss"));

  const QString destination =
      QDir(trash).filePath(stamp + QChar('-') +
                           QFileInfo(absolute).fileName());

  if (!QFile::rename(absolute, destination)) {
    return makeError(
        QStringLiteral("Could not move the note to the trash."));
  }

  if (context.scopeIndex) {
    context.scopeIndex->removeFile(absolute);
  }

  const QString relative = QDir(notesRoot).relativeFilePath(absolute);

  return makeOk(
      QStringLiteral("Moved %1 to the trash.").arg(relative));
}

// ---------------------------------------------------------------------
// Installation
// ---------------------------------------------------------------------

void NoteTools::installAll(AssistantToolRegistry &registry) {
  registry.registerTool(std::make_unique<ListNotesTool>());
  registry.registerTool(std::make_unique<ReadNoteTool>());
  registry.registerTool(std::make_unique<WriteNoteTool>());
  registry.registerTool(std::make_unique<EditNoteTool>());
  registry.registerTool(std::make_unique<DeleteNoteTool>());
}

} // namespace assistant