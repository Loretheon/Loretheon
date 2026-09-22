#include "../../include/overseer/EditNoteTool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

namespace {

// Returns a non-colliding path inside `outputFolder` that keeps the
// original basename. If "foo.md" exists, tries "foo-2.md", "foo-3.md"...
QString uniqueCopyPath(const QString &outputFolder,
                       const QString &basename) {
  QDir dir(outputFolder);

  const QString stem = QFileInfo(basename).completeBaseName();
  const QString suffix = QFileInfo(basename).suffix();

  const auto compose = [&](int n) {
    if (n <= 1) {
      return suffix.isEmpty() ? stem
                              : QStringLiteral("%1.%2").arg(stem, suffix);
    }
    return suffix.isEmpty()
               ? QStringLiteral("%1-%2").arg(stem).arg(n)
               : QStringLiteral("%1-%2.%3").arg(stem).arg(n).arg(suffix);
  };

  QString candidate = dir.filePath(compose(1));

  for (int n = 2; QFileInfo::exists(candidate); ++n) {
    candidate = dir.filePath(compose(n));
  }

  return candidate;
}

bool isUnderRoot(const QString &path, const QString &root) {
  if (root.isEmpty()) {
    return false;
  }

  const QString canonicalRoot = QFileInfo(root).absoluteFilePath();
  const QString canonicalPath = QFileInfo(path).absoluteFilePath();

  if (canonicalPath == canonicalRoot) {
    return true;
  }

  return canonicalPath.startsWith(canonicalRoot + QDir::separator());
}

} // namespace

QJsonObject EditNoteTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Absolute path to a note under the notes root."));

  QJsonObject instruction;
  instruction.insert(QStringLiteral("type"), QStringLiteral("string"));
  instruction.insert(QStringLiteral("description"),
                     QStringLiteral("What the user should see changed in "
                                    "the note. Write as if instructing a "
                                    "human editor."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);
  properties.insert(QStringLiteral("instruction"), instruction);

  QJsonArray required;
  required.append(QStringLiteral("path"));
  required.append(QStringLiteral("instruction"));

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"), required);

  return schema;
}

Tool::Result EditNoteTool::execute(const QJsonObject &arguments,
                                           const Context &context) const {
  Result result;

  const QString path =
      arguments.value(QStringLiteral("path")).toString().trimmed();
  const QString instruction =
      arguments.value(QStringLiteral("instruction")).toString().trimmed();

  if (path.isEmpty() || instruction.isEmpty()) {
    result.ok = false;
    result.error = QStringLiteral("Both 'path' and 'instruction' are required.");
    return result;
  }

  if (!isUnderRoot(path, context.notesRoot)) {
    result.ok = false;
    result.error = QStringLiteral(
        "Path is not under the notes root. edit_note only works on notes.");
    return result;
  }

  const QFileInfo info(path);

  if (!info.exists() || !info.isFile()) {
    result.ok = false;
    result.error = QStringLiteral("File does not exist: %1").arg(path);
    return result;
  }

  if (context.outputFolder.isEmpty() ||
      !QDir(context.outputFolder).exists()) {
    result.ok = false;
    result.error = QStringLiteral("Session output folder is unavailable.");
    return result;
  }

  const QString copyPath = uniqueCopyPath(context.outputFolder, info.fileName());

  if (!QFile::copy(info.absoluteFilePath(), copyPath)) {
    result.ok = false;
    result.error = QStringLiteral("Could not copy note into the session.");
    return result;
  }

  if (!context.requestEditNoteReview) {
    // Roll back: no review surface available.
    QFile::remove(copyPath);

    result.ok = false;
    result.error = QStringLiteral(
        "No review surface is available to open the edit.");
    return result;
  }

  context.requestEditNoteReview(copyPath, info.absoluteFilePath(),
                                instruction);

  const QString relativeCopy = QDir(context.sessionFolder)
                                   .relativeFilePath(copyPath);

  result.ok = true;
  result.output =
      QStringLiteral("Opened %1 for review. The user will apply changes "
                     "manually; the original note is not modified.")
          .arg(relativeCopy);
  return result;
}