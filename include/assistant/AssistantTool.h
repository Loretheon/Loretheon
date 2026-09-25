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
class OverseerSessionManager;
class NotePromoter;
class ScopeIndex;
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
  OverseerSessionManager *overseerManager = nullptr;
  NotePromoter *promoter = nullptr;
  ScopeIndex *scopeIndex = nullptr;
  // The last few activity events, for tools that want to reason about
  // what just happened.
  QStringList recentActivity;

  // Called by a tool that wants the user to review before acting.
  // Returns true if the action was approved. Null means proceed
  // without asking.
  std::function<bool(const QString &title, const QString &body)> requestReview;
};

// The interface every assistant tool implements.
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

  virtual bool isDestructive() const { return false; }

  virtual QJsonObject parametersSchema() const = 0;

  virtual Result execute(const QJsonObject &arguments,
                         const AssistantToolContext &context) const = 0;

  QJsonObject toSchema() const;
};

} // namespace assistant