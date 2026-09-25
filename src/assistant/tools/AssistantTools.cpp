#include "../../../include/assistant/tools/AssistantTools.h"

#include "../../../include/assistant/AssistantMemory.h"
#include "../../../include/assistant/AssistantProfile.h"
#include "../../../include/assistant/AssistantToolRegistry.h"
#include "../../../include/overseer/OverseerSessionManager.h"
#include "../../../include/search/SearchService.h"
#include "NotePromoter.h"
#include "OverseerRunner.h"
#include "OverseerSession.h"
#include "Settings.h"
#include "inference/InferenceService.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

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
  const QString query =
      arguments.value(QStringLiteral("query")).toString().trimmed();

  if (query.isEmpty()) {
    return makeError(QStringLiteral("'query' is required."));
  }

  if (!context.search || !context.search->isReady()) {
    return makeError(QStringLiteral("Search index is not ready."));
  }

  // The assistant's search runs through the RetrievalLoop that
  // LoreAssistant owns, not through this tool. This tool only exists
  // so the model can signal intent. The conductor in LoreAssistant
  // intercepts the call and does the work; the tool itself returns a
  // marker that the conductor recognises.
  return makeOk(QStringLiteral("__lore_search__:") + query);
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
// RememberFactTool
// ---------------------------------------------------------------------

QString RememberFactTool::description() const {
  return QStringLiteral(
      "Remember a fact. Use this when the user tells you something "
      "about themselves or about you that should persist. The scope "
      "argument decides where it goes: 'user' for facts about the "
      "user, 'self' for facts about you, 'memory' for everything "
      "else. The fact is written immediately; the user does not need "
      "to approve it.");
}

QJsonObject RememberFactTool::parametersSchema() const {
  QJsonObject scope;
  scope.insert(QStringLiteral("type"), QStringLiteral("string"));
  scope.insert(
      QStringLiteral("description"),
      QStringLiteral("Where the fact belongs. One of: 'user' (a fact "
                     "about the user), 'self' (a fact about you), "
                     "'memory' (a topic fact that may grow)."));

  QJsonObject topic;
  topic.insert(QStringLiteral("type"), QStringLiteral("string"));
  topic.insert(
      QStringLiteral("description"),
      QStringLiteral("Short topic name, e.g. 'programming'. Required "
                     "only when scope is 'memory'."));

  QJsonObject fact;
  fact.insert(QStringLiteral("type"), QStringLiteral("string"));
  fact.insert(QStringLiteral("description"),
              QStringLiteral("A single sentence stating the fact."));

  QJsonObject properties;
  properties.insert(QStringLiteral("scope"), scope);
  properties.insert(QStringLiteral("topic"), topic);
  properties.insert(QStringLiteral("fact"), fact);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("scope"),
                           QStringLiteral("fact")});

  return schema;
}

AssistantTool::Result RememberFactTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  const QString scope =
      arguments.value(QStringLiteral("scope")).toString().trimmed().toLower();

  const QString fact =
      arguments.value(QStringLiteral("fact")).toString().trimmed();

  if (fact.isEmpty()) {
    return makeError(QStringLiteral("'fact' is empty."));
  }

  if (scope == QStringLiteral("user")) {
    if (!context.profile) {
      return makeError(QStringLiteral("Profile is not available."));
    }

    QString body = context.profile->user();

    if (!body.endsWith(QChar('\n'))) {
      body += QChar('\n');
    }

    body += QStringLiteral("\n- ");
    body += fact;
    body += QChar('\n');

    context.profile->setUser(body);

    if (!context.profile->save()) {
      return makeError(QStringLiteral("Could not write user.md."));
    }

    return makeOk(QStringLiteral("Noted about you: %1").arg(fact));
  }

  if (scope == QStringLiteral("self")) {
    if (!context.profile) {
      return makeError(QStringLiteral("Profile is not available."));
    }

    QString body = context.profile->self();

    if (!body.endsWith(QChar('\n'))) {
      body += QChar('\n');
    }

    body += QStringLiteral("\n- ");
    body += fact;
    body += QChar('\n');

    context.profile->setSelf(body);

    if (!context.profile->save()) {
      return makeError(QStringLiteral("Could not write self.md."));
    }

    return makeOk(QStringLiteral("Noted about me: %1").arg(fact));
  }

  if (scope == QStringLiteral("memory")) {
    if (!context.memory) {
      return makeError(QStringLiteral("Memory is not available."));
    }

    const QString topic =
        arguments.value(QStringLiteral("topic")).toString().trimmed();

    if (topic.isEmpty()) {
      return makeError(
          QStringLiteral("'topic' is required when scope is 'memory'."));
    }

    const QString slug = AssistantMemory::slugify(topic);

    if (!context.memory->appendToTopic(slug, fact)) {
      return makeError(QStringLiteral("Could not write to memory."));
    }

    return makeOk(QStringLiteral("Remembered under '%1'.").arg(slug));
  }

  return makeError(
      QStringLiteral("Unknown scope '%1'. Use 'user', 'self', or "
                     "'memory'.").arg(scope));
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

// ---------------------------------------------------------------------
// Installation
// ---------------------------------------------------------------------

void AssistantTools::installAll(AssistantToolRegistry &registry) {
  registry.registerTool(std::make_unique<SearchTool>());
  registry.registerTool(std::make_unique<DelegateTool>());
  registry.registerTool(std::make_unique<RememberFactTool>());
  registry.registerTool(std::make_unique<SpeakTool>());
  registry.registerTool(std::make_unique<PromoteNoteTool>());
}

} // namespace assistant