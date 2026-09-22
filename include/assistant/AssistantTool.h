#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>

class DocumentManager;
class InferenceService;
class SearchService;
class TextEdit;
class AvatarWidget;
class AssistantMemory;
class AssistantProfile;

namespace assistant {

// Everything a tool needs to act. Assembled by the conductor before
// each tool call. Any pointer may be null; tools must handle that.
struct AssistantToolContext {
  DocumentManager *documents = nullptr;
  TextEdit *editor = nullptr;
  InferenceService *inference = nullptr;
  SearchService *search = nullptr;
  AssistantMemory *memory = nullptr;
  AssistantProfile *profile = nullptr;
  AvatarWidget *avatar = nullptr;

  // The last few activity events, for tools that want to reason about
  // what just happened.
  QStringList recentActivity;

  // Called by a tool that wants the user to review before acting.
  // Returns true if the action was approved. In tests and in
  // autonomous mode the callback may be null, in which case the tool
  // must decide for itself whether to proceed.
  std::function<bool(const QString &title, const QString &body)> requestReview;
};

// The interface every assistant tool implements. Mirrors the shape of
// the Overseer Tool interface, but the context is different because
// the assistant has access to different things.
class AssistantTool {
public:
  struct Result {
    bool ok = false;
    QString output;
    QString error;
  };

  virtual ~AssistantTool() = default;

  virtual QString name() const = 0;
  virtual QString description() const = 0;
  virtual QString category() const = 0;

  // True when the tool can modify user data or settings. Destructive
  // tools require the review gate to be enabled, or the user to have
  // disabled it explicitly.
  virtual bool isDestructive() const { return false; }

  virtual QJsonObject parametersSchema() const = 0;

  virtual Result execute(const QJsonObject &arguments,
                         const AssistantToolContext &context) const = 0;

  QJsonObject toSchema() const;
};

} // namespace assistant