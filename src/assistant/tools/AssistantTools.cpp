#include "../../../include/assistant/tools/AssistantTools.h"

#include "../../../include/app/DocumentManager.h"
#include "../../../include/app/Settings.h"
#include "../../../include/assistant/AssistantMemory.h"
#include "../../../include/assistant/AssistantProfile.h"
#include "../../../include/assistant/AssistantToolRegistry.h"
#include "../../../include/avatar/AvatarWidget.h"
#include "../../../include/search/SearchService.h"
#include "../../../include/text/TextEdit.h"
#include "inference/InferenceService.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QTextCursor>

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

QString describeHits(const QVector<SearchHit> &hits, int maxLength) {
  QString text;

  int index = 1;

  for (const SearchHit &hit : hits) {
    text += QStringLiteral("%1. ").arg(index++);
    text += QFileInfo(hit.filePath).fileName();

    if (!hit.heading.isEmpty()) {
      text += QStringLiteral(" — ") + hit.heading;
    }

    text += QStringLiteral("\n   ");
    text += hit.body.left(200).simplified();
    text += QStringLiteral("\n");
  }

  return text.left(maxLength);
}

} // namespace

// ---------------------------------------------------------------------
// SearchNotesTool
// ---------------------------------------------------------------------

QString SearchNotesTool::description() const {
  return QStringLiteral(
      "Search the user's notes by meaning. Use this when the user asks "
      "about something that might be in their notes and you do not "
      "already know the answer. Returns the top matching notes with a "
      "short snippet from each.");
}

QJsonObject SearchNotesTool::parametersSchema() const {
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

AssistantTool::Result SearchNotesTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.search || !context.search->isReady()) {
    return makeError(QStringLiteral("Search index is not ready."));
  }

  const QString query =
      arguments.value(QStringLiteral("query")).toString().trimmed();

  if (query.isEmpty()) {
    return makeError(QStringLiteral("'query' is required."));
  }

  const QVector<SearchHit> hits = context.search->search(query, 6);

  if (hits.isEmpty()) {
    return makeOk(QStringLiteral("No notes matched that query."));
  }

  return makeOk(describeHits(hits, 1600));
}

// ---------------------------------------------------------------------
// OpenFileTool
// ---------------------------------------------------------------------

QString OpenFileTool::description() const {
  return QStringLiteral(
      "Open a note in the editor. Use this when you want to draw the "
      "user's attention to a specific note, or when the next step of "
      "your work depends on a file being open. The path must be an "
      "absolute path to a note that exists.");
}

QJsonObject OpenFileTool::parametersSchema() const {
  QJsonObject path;
  path.insert(QStringLiteral("type"), QStringLiteral("string"));
  path.insert(QStringLiteral("description"),
              QStringLiteral("Absolute path to the note."));

  QJsonObject properties;
  properties.insert(QStringLiteral("path"), path);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("path")});

  return schema;
}

AssistantTool::Result OpenFileTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.documents) {
    return makeError(QStringLiteral("No document manager available."));
  }

  const QString path =
      arguments.value(QStringLiteral("path")).toString().trimmed();

  if (path.isEmpty()) {
    return makeError(QStringLiteral("'path' is required."));
  }

  const QFileInfo info(path);

  if (!info.exists() || !info.isFile()) {
    return makeError(QStringLiteral("File does not exist: %1").arg(path));
  }

  if (!context.documents->openFile(info.absoluteFilePath())) {
    return makeError(QStringLiteral("Could not open the file."));
  }

  return makeOk(QStringLiteral("Opened %1.").arg(info.fileName()));
}

// ---------------------------------------------------------------------
// InsertTextTool
// ---------------------------------------------------------------------

QString InsertTextTool::description() const {
  return QStringLiteral(
      "Insert text at the cursor in the currently open note. This "
      "modifies the user's file. Use it only when the user has asked "
      "for something to be written, or when a review has been "
      "approved. Prefer asking first if you are unsure.");
}

QJsonObject InsertTextTool::parametersSchema() const {
  QJsonObject text;
  text.insert(QStringLiteral("type"), QStringLiteral("string"));
  text.insert(QStringLiteral("description"),
              QStringLiteral("The text to insert at the cursor."));

  QJsonObject properties;
  properties.insert(QStringLiteral("text"), text);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("text")});

  return schema;
}

AssistantTool::Result InsertTextTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.editor) {
    return makeError(QStringLiteral("No editor is focused."));
  }

  const QString text =
      arguments.value(QStringLiteral("text")).toString();

  if (text.isEmpty()) {
    return makeError(QStringLiteral("'text' is empty."));
  }

  if (context.requestReview) {
    const bool approved = context.requestReview(
        QStringLiteral("Insert text"),
        text.left(400));

    if (!approved) {
      return makeOk(QStringLiteral("Insert was declined."));
    }
  }

  QTextCursor cursor = context.editor->textCursor();

  if (cursor.isNull()) {
    return makeError(QStringLiteral("Cursor is not valid."));
  }

  cursor.insertText(text);

  return makeOk(QStringLiteral("Inserted %1 characters.")
                    .arg(text.size()));
}

// ---------------------------------------------------------------------
// RememberFactTool
// ---------------------------------------------------------------------

QString RememberFactTool::description() const {
  return QStringLiteral(
      "Propose a fact to remember. The fact is written to the "
      "assistant's memory under the given topic. Use this when you "
      "learn something durable about the user or about the world that "
      "should persist across sessions. The user is asked to approve "
      "the fact before it is written, unless they have turned the "
      "review gate off.");
}

QJsonObject RememberFactTool::parametersSchema() const {
  QJsonObject topic;
  topic.insert(QStringLiteral("type"), QStringLiteral("string"));
  topic.insert(QStringLiteral("description"),
               QStringLiteral("Short topic name, e.g. 'programming'."));

  QJsonObject fact;
  fact.insert(QStringLiteral("type"), QStringLiteral("string"));
  fact.insert(QStringLiteral("description"),
              QStringLiteral("A single sentence stating the fact."));

  QJsonObject properties;
  properties.insert(QStringLiteral("topic"), topic);
  properties.insert(QStringLiteral("fact"), fact);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("topic"),
                           QStringLiteral("fact")});

  return schema;
}

AssistantTool::Result RememberFactTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.memory) {
    return makeError(QStringLiteral("Memory is not available."));
  }

  const QString topic =
      arguments.value(QStringLiteral("topic")).toString().trimmed();

  const QString fact =
      arguments.value(QStringLiteral("fact")).toString().trimmed();

  if (topic.isEmpty() || fact.isEmpty()) {
    return makeError(
        QStringLiteral("Both 'topic' and 'fact' are required."));
  }

  if (context.requestReview) {
    const bool approved = context.requestReview(
        QStringLiteral("Remember: %1").arg(topic), fact);

    if (!approved) {
      return makeOk(QStringLiteral("Memory was declined."));
    }
  }

  const QString slug = AssistantMemory::slugify(topic);

  if (!context.memory->appendToTopic(slug, fact)) {
    return makeError(QStringLiteral("Could not write to memory."));
  }

  return makeOk(QStringLiteral("Remembered under '%1'.").arg(slug));
}

// ---------------------------------------------------------------------
// ChangeSettingTool
// ---------------------------------------------------------------------

QString ChangeSettingTool::description() const {
  return QStringLiteral(
      "Change one of the assistant's own settings. You may only make a "
      "setting more restrictive, never less. If the user asks you to "
      "loosen a restriction, refuse and tell them to do it themselves "
      "in the settings panel. Field names: autonomy, speakResponses, "
      "listenMode, accessSearch, accessTools, accessUserMemory, "
      "accessSelfMemory, reviewGate, activityWatch.");
}

QJsonObject ChangeSettingTool::parametersSchema() const {
  QJsonObject field;
  field.insert(QStringLiteral("type"), QStringLiteral("string"));
  field.insert(QStringLiteral("description"),
               QStringLiteral("The setting to change."));

  QJsonObject value;
  value.insert(QStringLiteral("type"), QStringLiteral("integer"));
  value.insert(
      QStringLiteral("description"),
      QStringLiteral("The new numeric value. Booleans use 0 or 1. "
                     "Enums use their integer rank."));

  QJsonObject properties;
  properties.insert(QStringLiteral("field"), field);
  properties.insert(QStringLiteral("value"), value);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("field"),
                           QStringLiteral("value")});

  return schema;
}

AssistantTool::Result ChangeSettingTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  Q_UNUSED(context);

  const QString field =
      arguments.value(QStringLiteral("field")).toString().trimmed();

  const int value = arguments.value(QStringLiteral("value")).toInt();

  if (field.isEmpty()) {
    return makeError(QStringLiteral("'field' is required."));
  }

  if (Settings::assistantWriteDirectionFor(field) ==
      Settings::AssistantWriteDirection::None) {
    return makeError(
        QStringLiteral("Field '%1' is not writable by the assistant.")
            .arg(field));
  }

  QString reason;

  if (!Settings::setAssistantFieldRestricted(field, value, &reason)) {
    return makeError(reason);
  }

  return makeOk(
      QStringLiteral("Set %1 to %2.")
          .arg(field, Settings::describeAssistantField(field)));
}

// ---------------------------------------------------------------------
// SpeakTool
// ---------------------------------------------------------------------

QString SpeakTool::description() const {
  return QStringLiteral(
      "Speak a line aloud through the user's speakers. Use this when "
      "you want to say something that does not need to appear in the "
      "chat transcript, or when the user has asked you to read "
      "something. Speech is animated on the avatar if it is visible.");
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

// ---------------------------------------------------------------------
// SetExpressionTool
// ---------------------------------------------------------------------

QString SetExpressionTool::description() const {
  return QStringLiteral(
      "Change the avatar's facial expression. The expression persists "
      "until you change it again or the next utterance begins. Use it "
      "to react to the conversation. Valid names depend on the model "
      "currently loaded.");
}

QJsonObject SetExpressionTool::parametersSchema() const {
  QJsonObject name;
  name.insert(QStringLiteral("type"), QStringLiteral("string"));
  name.insert(QStringLiteral("description"),
              QStringLiteral("The expression name."));

  QJsonObject properties;
  properties.insert(QStringLiteral("name"), name);

  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  schema.insert(QStringLiteral("properties"), properties);
  schema.insert(QStringLiteral("required"),
                QJsonArray{QStringLiteral("name")});

  return schema;
}

AssistantTool::Result SetExpressionTool::execute(
    const QJsonObject &arguments,
    const AssistantToolContext &context) const {
  if (!context.avatar) {
    return makeError(QStringLiteral("Avatar is not available."));
  }

  const QString name =
      arguments.value(QStringLiteral("name")).toString().trimmed();

  if (name.isEmpty()) {
    return makeError(QStringLiteral("'name' is empty."));
  }

  context.avatar->setExpression(name);

  return makeOk(QStringLiteral("Expression set to '%1'.").arg(name));
}

// ---------------------------------------------------------------------
// Installation
// ---------------------------------------------------------------------

void AssistantTools::installAll(AssistantToolRegistry &registry) {
  registry.registerTool(std::make_unique<SearchNotesTool>());
  registry.registerTool(std::make_unique<OpenFileTool>());
  registry.registerTool(std::make_unique<InsertTextTool>());
  registry.registerTool(std::make_unique<RememberFactTool>());
  registry.registerTool(std::make_unique<ChangeSettingTool>());
  registry.registerTool(std::make_unique<SpeakTool>());
  registry.registerTool(std::make_unique<SetExpressionTool>());
}

} // namespace assistant