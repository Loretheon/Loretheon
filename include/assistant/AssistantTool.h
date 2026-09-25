#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
class LoreAssistant;
class DocumentManager;
class InferenceService;
class SearchService;
class TextEdit;
class AvatarWidget;
class AssistantMemory;
class AssistantProfile;
class NotePromoter;
class OverseerSessionManager;
class ScopeIndex;

namespace assistant {

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
  LoreAssistant *assistant = nullptr;
  QString notesRoot;

  QStringList recentActivity;

  std::function<bool(const QString &title, const QString &body)> requestReview;
};

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