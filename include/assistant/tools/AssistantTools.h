#pragma once

#include "../AssistantTool.h"

namespace assistant {
class AssistantToolRegistry;

// The tools Lore uses. She does not implement anything herself: she
// dispatches into the surfaces that already exist, and edits her own
// files through the scoped-edit pipeline.
//
//   search        — runs a query through the user's notes and returns
//                   the synthesised answer. Runs on the assistant's
//                   own RetrievalLoop, independent of the Search page.
//
//   delegate      — hands a task to the Overseer conductor. The
//                   conductor runs it in a session and the runner
//                   reports back when it is done. The assistant does
//                   not wait.
//
//   edit_profile  — runs a scoped edit against one of the assistant's
//                   own files: identity.md, user.md, self.md, or a
//                   topic file under memories/topics/. Returns a job
//                   id. No user review.
//
//   read_paste    — reads the full text of a large paste the user
//                   dropped into the composer. Pastes are never sent
//                   inline; the message carries a placeholder with the
//                   paste id, and this tool fetches the body on
//                   demand.
//
//   speak         — speaks a line through the user's speakers.


class DelegateTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("delegate"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("write"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class EditProfileTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("edit_profile"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("write"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class ReadPasteTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("read_paste"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class SpeakTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("speak"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("voice"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class PromoteNoteTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("promote_note"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("write"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class SearchTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("search"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class ReadJobTool : public AssistantTool {
public:
  QString name() const override { return QStringLiteral("read_job"); }
  QString description() const override;
  QString category() const override { return QStringLiteral("read"); }
  QJsonObject parametersSchema() const override;
  Result execute(const QJsonObject &arguments,
                 const AssistantToolContext &context) const override;
};

class AssistantTools {
public:
  static void installAll(AssistantToolRegistry &registry);
};


} // namespace assistant