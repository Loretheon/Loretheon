#include "../../../include/assistant/tools/AssistantTools.h"

#include "../../../include/assistant/AssistantMemory.h"
#include "../../../include/assistant/AssistantProfile.h"
#include "../../../include/assistant/AssistantToolRegistry.h"
#include "../../../include/overseer/OverseerSessionManager.h"
#include "../../../include/search/SearchService.h"
#include "LoreAssistant.h"
#include "NotePromoter.h"
#include "OverseerRunner.h"
#include "OverseerSession.h"
#include "Settings.h"
#include "inference/InferenceService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTextStream>

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

} // namespace

// ---------------------------------------------------------------------
// SearchTool
// ---------------------------------------------------------------------

QString SearchTool::description() const {
  return QStringLiteral(
      "Search the user's notes by meaning and return a synthesised "
      "answer with citations. Use this when the user asks about "
      "something that might be in their notes and you do not already "
      "know the answer. The answer is produced from the notes "
      "themselves, so cite it rather than paraphrasing.");
}

QJsonObject SearchTool::parametersSchema() const {
  QJsonObject query;
  query.insert(QStringLiteral("type"), QStringLiteral("string"));
  query.insert(QStringLiteral("description"),
               QStringLiteral("What to look for."));

  QJsonObject properties;
  properties.insert(QStringLiteral("query"), query);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("query")});

  return schema;
}

AssistantTool::Result SearchTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  Q_UNUSED(context);

  const QString query =
      arguments.value(QStringLiteral("query")).toString().trimmed();

  if (query.isEmpty()) {
    return makeError(QStringLiteral("'query' is required."));
  }

  return makeOk(QStringLiteral("__job_search__:") + query);
}

// ---------------------------------------------------------------------
// DelegateTool
// ---------------------------------------------------------------------

QString DelegateTool::description() const {
  return QStringLiteral(
      "Hand a task to the worker. The worker creates and modifies "
      "notes.\n"
      "\n"
      "There are three ways to call this:\n"
      "\n"
      "  1. With a session name you already know. Use this when you "
      "have just been shown the session list, or when the user named "
      "a session explicitly.\n"
      "\n"
      "  2. With create=true, a name, and a description. Use this "
      "when no existing session fits and the task deserves its own. "
      "The description must be a short, intentional sentence that "
      "says what the session is about. It will be read later by you "
      "when you are choosing where other tasks belong.\n"
      "\n"
      "  3. With no session at all. The application will show you "
      "the list of existing sessions and their descriptions, and you "
      "will be asked again to pick one.\n"
      "\n"
      "The task runs in the background. Reply to the user with a "
      "short acknowledgement and stop.");
}

QJsonObject DelegateTool::parametersSchema() const {
  QJsonObject instruction;
  instruction.insert(QStringLiteral("type"), QStringLiteral("string"));
  instruction.insert(
      QStringLiteral("description"),
      QStringLiteral("What to produce, in the user's own words where "
                     "possible. The worker sees this verbatim."));

  QJsonObject session;
  session.insert(QStringLiteral("type"), QStringLiteral("string"));
  session.insert(
      QStringLiteral("description"),
      QStringLiteral("The session to run the task in. Leave empty to "
                     "be shown the session list, or to create a new "
                     "one."));

  QJsonObject create;
  create.insert(QStringLiteral("type"), QStringLiteral("boolean"));
  create.insert(
      QStringLiteral("description"),
      QStringLiteral("Set to true to create a new session. Requires "
                     "'session' as the name and 'description' as the "
                     "intentional description."));

  QJsonObject description;
  description.insert(QStringLiteral("type"), QStringLiteral("string"));
  description.insert(
      QStringLiteral("description"),
      QStringLiteral("Only when create is true. A short, "
                     "intentional sentence describing what this "
                     "session is about. Read later when choosing "
                     "where other tasks belong."));

  QJsonObject properties;
  properties.insert(QStringLiteral("instruction"), instruction);
  properties.insert(QStringLiteral("session"), session);
  properties.insert(QStringLiteral("create"), create);
  properties.insert(QStringLiteral("description"), description);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("instruction")});

  return schema;
}

AssistantTool::Result DelegateTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.overseerManager) {
    return makeError(QStringLiteral("The worker is not available."));
  }

  const QString instruction =
      arguments.value(QStringLiteral("instruction")).toString().trimmed();

  if (instruction.isEmpty()) {
    return makeError(QStringLiteral("'instruction' is required."));
  }

  const QString session =
      arguments.value(QStringLiteral("session")).toString().trimmed();

  const bool create =
      arguments.value(QStringLiteral("create")).toBool(false);

  const QString description =
      arguments.value(QStringLiteral("description")).toString().trimmed();

  if (create) {
    if (session.isEmpty()) {
      return makeError(
          QStringLiteral("'session' is required when create is true."));
    }

    if (description.isEmpty()) {
      return makeError(
          QStringLiteral("'description' is required when create is "
                         "true."));
    }

    if (context.overseerManager->sessionExists(session)) {
      return makeError(
          QStringLiteral("A session named '%1' already exists.")
              .arg(session));
    }

    if (!context.overseerManager->createSession(session, description)) {
      return makeError(
          QStringLiteral("Could not create session '%1'.").arg(session));
    }

    const QString requestId = context.overseerManager->submitToSession(
        session, instruction, Origin::Lore);

    if (requestId.isEmpty()) {
      return makeError(
          QStringLiteral("The session was created but the task could "
                         "not be submitted."));
    }

    return makeOk(QStringLiteral("__lore_delegated__:") + requestId +
                  QStringLiteral(":") + session);
  }

  if (session.isEmpty()) {
    return makeOk(QStringLiteral("__lore_needs_session__:") + instruction);
  }

  if (!context.overseerManager->sessionExists(session)) {
    return makeError(
        QStringLiteral("No session named '%1'.").arg(session));
  }

  const QString requestId = context.overseerManager->submitToSession(
      session, instruction, Origin::Lore);

  if (requestId.isEmpty()) {
    return makeError(QStringLiteral("The worker could not accept the task."));
  }

  return makeOk(QStringLiteral("__lore_delegated__:") + requestId +
                QStringLiteral(":") + session);
}

// ---------------------------------------------------------------------
// EditProfileTool
// ---------------------------------------------------------------------

QString EditProfileTool::description() const {
  return QStringLiteral(
      "Edit one of your own files with a scoped edit. The target is "
      "loaded, edited by the scoped-edit pipeline, and written back. "
      "There is no user review. Use this to remember facts, revise "
      "your understanding of the user, adjust your own character, or "
      "update a topic file.\n"
      "\n"
      "Targets:\n"
      "  'identity' — identity.md, who you are. Use sparingly. Changes "
      "take effect on your next turn.\n"
      "  'user'     — user.md, what you know about the user.\n"
      "  'self'     — self.md, what you know about yourself.\n"
      "  'topic'    — a topic file under memories/topics/. Requires "
      "'topic' as a short name. Creates the file if it does not exist.\n"
      "\n"
      "The instruction is a plain-language description of the change, "
      "as if instructing a human editor. Examples: 'add that the user "
      "prefers British spelling', 'replace the Known section's first "
      "line with the user's name', 'append a note about the project "
      "they mentioned today'. The pipeline chooses the edit. Do not "
      "attempt to write the whole file.\n"
      "\n"
      "Returns a job id. The edit runs in the background. Call "
      "read_job to see the result.");
}

QJsonObject EditProfileTool::parametersSchema() const {
  QJsonObject target;
  target.insert(QStringLiteral("type"), QStringLiteral("string"));
  target.insert(
      QStringLiteral("description"),
      QStringLiteral("One of: 'identity', 'user', 'self', 'topic'."));

  QJsonObject topic;
  topic.insert(QStringLiteral("type"), QStringLiteral("string"));
  topic.insert(
      QStringLiteral("description"),
      QStringLiteral("Short topic name. Required only when target is "
                     "'topic'. Normalised to a filesystem slug."));

  QJsonObject instruction;
  instruction.insert(QStringLiteral("type"), QStringLiteral("string"));
  instruction.insert(
      QStringLiteral("description"),
      QStringLiteral("A plain-language description of the edit."));

  QJsonObject properties;
  properties.insert(QStringLiteral("target"), target);
  properties.insert(QStringLiteral("topic"), topic);
  properties.insert(QStringLiteral("instruction"), instruction);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("target"),
                           QStringLiteral("instruction")});

  return schema;
}

AssistantTool::Result EditProfileTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.profile) {
    return makeError(QStringLiteral("The profile is not available."));
  }

  const QString target =
      arguments.value(QStringLiteral("target")).toString().trimmed().toLower();

  const QString instruction =
      arguments.value(QStringLiteral("instruction")).toString().trimmed();

  if (instruction.isEmpty()) {
    return makeError(QStringLiteral("'instruction' is required."));
  }

  QString path;

  if (target == QStringLiteral("identity")) {
    path = context.profile->identityPath();
  } else if (target == QStringLiteral("user")) {
    path = context.profile->userPath();
  } else if (target == QStringLiteral("self")) {
    path = context.profile->selfPath();
  } else if (target == QStringLiteral("topic")) {
    if (!context.memory) {
      return makeError(QStringLiteral("Memory is not available."));
    }

    const QString topic =
        arguments.value(QStringLiteral("topic")).toString().trimmed();

    if (topic.isEmpty()) {
      return makeError(
          QStringLiteral("'topic' is required when target is 'topic'."));
    }

    const QString slug = AssistantMemory::slugify(topic);
    path = context.memory->topicPath(slug);

    if (!QFileInfo::exists(path)) {
      if (!context.memory->appendToTopic(slug, QStringLiteral("(empty)"))) {
        return makeError(
            QStringLiteral("Could not create the topic file."));
      }
    }
  } else {
    return makeError(
        QStringLiteral("Unknown target '%1'. Use 'identity', 'user', "
                       "'self', or 'topic'.").arg(target));
  }

  if (path.isEmpty()) {
    return makeError(QStringLiteral("Could not resolve the target path."));
  }

  QString encoded = instruction;
  encoded.replace(QChar('\\'), QStringLiteral("\\\\"));
  encoded.replace(QChar('\n'), QStringLiteral("\\n"));

  return makeOk(QStringLiteral("__job_profile_edit__:") + path +
                QStringLiteral("\n") + encoded);
}

// ---------------------------------------------------------------------
// ReadPasteTool
// ---------------------------------------------------------------------

QString ReadPasteTool::description() const {
  return QStringLiteral(
      "Read the full text of a paste the user dropped into the "
      "composer. The user's message carries a placeholder like "
      "[paste: config.yaml, 4821 chars — id a3f19c]; the full body is "
      "on disk and is not sent to you automatically. Call this only "
      "when you actually need the contents. The id is the last token "
      "of the placeholder.");
}

QJsonObject ReadPasteTool::parametersSchema() const {
  QJsonObject id;
  id.insert(QStringLiteral("type"), QStringLiteral("string"));
  id.insert(QStringLiteral("description"),
            QStringLiteral("The paste id from the placeholder."));

  QJsonObject properties;
  properties.insert(QStringLiteral("id"), id);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("id")});

  return schema;
}

AssistantTool::Result ReadPasteTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (context.root.isEmpty()) {
    return makeError(QStringLiteral("The assistant root is not available."));
  }

  const QString id =
      arguments.value(QStringLiteral("id")).toString().trimmed();

  if (id.isEmpty()) {
    return makeError(QStringLiteral("'id' is required."));
  }

  static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_-]+$"));

  if (!safe.match(id).hasMatch()) {
    return makeError(QStringLiteral("Invalid paste id."));
  }

  const QString path = QDir(context.root)
                           .filePath(QStringLiteral("pastes/%1.txt").arg(id));

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return makeError(
        QStringLiteral("No paste with id '%1'.").arg(id));
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString body = stream.readAll();

  if (stream.status() != QTextStream::Ok) {
    return makeError(QStringLiteral("Could not read the paste."));
  }

  return makeOk(body);
}

// ---------------------------------------------------------------------
// SpeakTool
// ---------------------------------------------------------------------

QString SpeakTool::description() const {
  return QStringLiteral(
      "Speak a line aloud through the user's speakers. Use this when "
      "the user has asked you to read something, or when a spoken "
      "line is more natural than text. Speech is animated on the "
      "avatar when it is visible.");
}

QJsonObject SpeakTool::parametersSchema() const {
  QJsonObject text;
  text.insert(QStringLiteral("type"), QStringLiteral("string"));
  text.insert(QStringLiteral("description"),
              QStringLiteral("The text to speak."));

  QJsonObject properties;
  properties.insert(QStringLiteral("text"), text);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("text")});

  return schema;
}

AssistantTool::Result SpeakTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.inference) {
    return makeError(QStringLiteral("Inference is unavailable."));
  }

  const QString text =
      arguments.value(QStringLiteral("text")).toString().trimmed();

  if (text.isEmpty()) {
    return makeError(QStringLiteral("'text' is empty."));
  }

  context.inference->speak(text);

  return makeOk(QStringLiteral("Spoke."));
}


QString PromoteNoteTool::description() const {
  return QStringLiteral(
      "Copy a file the worker produced into the user's notes. Use "
      "this when a generated file is worth keeping permanently. The "
      "file is copied, not moved. It lands under a folder named "
      "after the session. If a file with the same name already "
      "exists in the notes folder, it is skipped rather than "
      "overwritten.");
}

QJsonObject PromoteNoteTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(
      QStringLiteral("description"),
      QStringLiteral("Path to the file or folder inside the session's "
                     "output, relative to the session root. For "
                     "example 'cv/Sujan_CV.md' or 'projects'."));

  QJsonObject session;
  session.insert(QStringLiteral("type"), QStringLiteral("string"));
  session.insert(
      QStringLiteral("description"),
      QStringLiteral("The session the file came from. Leave empty to "
                     "use the current session."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);
  properties.insert(QStringLiteral("session"), session);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("path")});

  return schema;
}

AssistantTool::Result PromoteNoteTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.overseerManager) {
    return makeError(QStringLiteral("The worker is not available."));
  }

  if (!context.promoter) {
    return makeError(QStringLiteral("The promoter is not available."));
  }

  const QString relativePath =
      arguments.value(QStringLiteral("path")).toString().trimmed();

  if (relativePath.isEmpty()) {
    return makeError(QStringLiteral("'path' is required."));
  }

  QString session =
      arguments.value(QStringLiteral("session")).toString().trimmed();

  if (session.isEmpty()) {
    session = context.overseerManager->activeSessionName();
  }

  if (session.isEmpty()) {
    return makeError(
        QStringLiteral("No session is open and none was named."));
  }

  OverseerRunner *runner = context.overseerManager->runner(session);

  if (!runner || !runner->session()) {
    return makeError(
        QStringLiteral("Session '%1' is not open.").arg(session));
  }

  const QString sessionOutput = runner->session()->outputPath();

  const QString absolute =
      QDir(sessionOutput).absoluteFilePath(relativePath);

  if (!QFileInfo::exists(absolute)) {
    return makeError(
        QStringLiteral("No such file in session '%1': %2")
            .arg(session, relativePath));
  }

  const QString notesRoot = Settings::getRootDirectory();

  const NotePromoter::Result result =
      context.promoter->promote(absolute, session, notesRoot);

  if (!result.ok()) {
    return makeError(result.error);
  }

  QString summary;

  if (result.written.isEmpty() && result.skipped.isEmpty() &&
      result.copiedNotIndexed.isEmpty()) {
    return makeOk(QStringLiteral(
        "Nothing was promoted. The path may not contain any "
        "markdown files."));
      }

  if (!result.written.isEmpty()) {
    summary += QStringLiteral("Promoted %1 file(s) into notes/%2.")
                   .arg(result.written.size())
                   .arg(session);
  }

  if (!result.skipped.isEmpty()) {
    if (!summary.isEmpty())
      summary += QStringLiteral(" ");

    summary += QStringLiteral("Skipped %1 file(s) that already "
                              "existed.")
                   .arg(result.skipped.size());
  }

  if (!result.copiedNotIndexed.isEmpty()) {
    if (!summary.isEmpty())
      summary += QStringLiteral(" ");

    summary += QStringLiteral("%1 file(s) were copied but not indexed; "
                              "the search index may need a rebuild.")
                   .arg(result.copiedNotIndexed.size());
  }

  if (!result.failed.isEmpty()) {
    if (!summary.isEmpty())
      summary += QStringLiteral(" ");

    summary += QStringLiteral("Failed on %1 file(s).")
                   .arg(result.failed.size());
  }

  return makeOk(summary);
}

QString ReadJobTool::description() const {
  return QStringLiteral(
      "Read the result of a job by id. Returns immediately if the job "
      "is done. Waits if it is still running. Returns an error if the "
      "job failed, was cancelled, or the user aborted the wait. Only "
      "call this for a job you started and actually need the result "
      "of.");
}

QJsonObject ReadJobTool::parametersSchema() const {
  QJsonObject id;
  id.insert(QStringLiteral("type"), QStringLiteral("string"));
  id.insert(QStringLiteral("description"),
            QStringLiteral("The job id returned when the job started."));

  QJsonObject properties;
  properties.insert(QStringLiteral("id"), id);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("id")});

  return schema;
}

AssistantTool::Result ReadJobTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.assistant) {
    return makeError(QStringLiteral("The assistant is not available."));
  }

  const QString jobId =
      arguments.value(QStringLiteral("id")).toString().trimmed();

  if (jobId.isEmpty()) {
    return makeError(QStringLiteral("'id' is required."));
  }

  QString result;
  QString error;

  if (!context.assistant->waitForJob(jobId, &result, &error)) {
    return makeError(error);
  }

  return makeOk(result);
}


// ---------------------------------------------------------------------
// Installation
// ---------------------------------------------------------------------

void AssistantTools::installAll(AssistantToolRegistry &registry) {
  registry.registerTool(std::make_unique<SearchTool>());
  registry.registerTool(std::make_unique<DelegateTool>());
  registry.registerTool(std::make_unique<EditProfileTool>());
  registry.registerTool(std::make_unique<ReadPasteTool>());
  registry.registerTool(std::make_unique<SpeakTool>());
  registry.registerTool(std::make_unique<PromoteNoteTool>());
  registry.registerTool(std::make_unique<ReadJobTool>());
}

} // namespace assistant