#include "Settings.h"

#include <QDir>
#include <QHash>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace {

constexpr auto OverseerToolCallDepthKey = "overseer/toolCallDepthLimit";
constexpr int DefaultToolCallDepth = 16;

constexpr auto OverseerConcurrencyCapKey = "overseer/concurrencyCap";
constexpr int DefaultConcurrencyCap = 4;

constexpr auto LlmModeKey = "llm/mode";
constexpr auto LlmEndpointKey = "llm/endpoint";
constexpr auto LlmModelKey = "llm/model";
constexpr auto LlmApiKeyKey = "llm/apiKey";
constexpr auto LlmAuthTypeKey = "llm/authType";
constexpr auto LlmSeededKey = "llm/seededFromEnv";

constexpr auto OverseerToastDurationKey = "overseer/toastDurationMs";
constexpr int DefaultToastDurationMs = 8000;

constexpr auto OverseerFileAgentCapKey = "overseer/fileAgentCap";
constexpr int DefaultFileAgentCap = 4;

// Assistant settings keys. All under the assistant/ prefix.
constexpr auto AssistantAutonomyKey = "assistant/autonomy";
constexpr auto AssistantSpeakResponsesKey = "assistant/speakResponses";
constexpr auto AssistantListenModeKey = "assistant/listenMode";
constexpr auto AssistantAccessSearchKey = "assistant/accessSearch";
constexpr auto AssistantAccessToolsKey = "assistant/accessTools";
constexpr auto AssistantAccessUserMemoryKey = "assistant/accessUserMemory";
constexpr auto AssistantAccessSelfMemoryKey = "assistant/accessSelfMemory";
constexpr auto AssistantReviewGateKey = "assistant/reviewGate";
constexpr auto AssistantActivityWatchKey = "assistant/activityWatch";

// The set of fields the assistant may write, mapped to the direction
// it may move them in. A field not in this map is None.
QHash<QString, Settings::AssistantWriteDirection>
buildAssistantWriteTable() {
  QHash<QString, Settings::AssistantWriteDirection> table;

  // Every field the assistant can write is MoreRestrictive. There are
  // no fields it can loosen. This is a deliberate design property:
  // the assistant can protect the user, it cannot unprotect them.
  table.insert(QStringLiteral("autonomy"),
               Settings::AssistantWriteDirection::MoreRestrictive);
  table.insert(QStringLiteral("speakResponses"),
               Settings::AssistantWriteDirection::MoreRestrictive);
  table.insert(QStringLiteral("listenMode"),
               Settings::AssistantWriteDirection::MoreRestrictive);
  table.insert(QStringLiteral("accessSearch"),
               Settings::AssistantWriteDirection::MoreRestrictive);
  table.insert(QStringLiteral("accessTools"),
               Settings::AssistantWriteDirection::MoreRestrictive);
  table.insert(QStringLiteral("accessUserMemory"),
               Settings::AssistantWriteDirection::MoreRestrictive);
  table.insert(QStringLiteral("accessSelfMemory"),
               Settings::AssistantWriteDirection::MoreRestrictive);
  table.insert(QStringLiteral("reviewGate"),
               Settings::AssistantWriteDirection::MoreRestrictive);
  table.insert(QStringLiteral("activityWatch"),
               Settings::AssistantWriteDirection::MoreRestrictive);

  return table;
}

const QHash<QString, Settings::AssistantWriteDirection>
    &assistantWriteTable() {
  static const QHash<QString, Settings::AssistantWriteDirection> table =
      buildAssistantWriteTable();
  return table;
}

// Read the current numeric value of a field from the settings store.
// Returns false if the field name is not recognised.
bool readAssistantFieldRaw(QSettings &settings, const QString &field,
                           int &value) {
  if (field == QStringLiteral("autonomy")) {
    value = settings
                .value(AssistantAutonomyKey,
                       static_cast<int>(Settings::AutonomyLevel::Suggestive))
                .toInt();
    return true;
  }

  if (field == QStringLiteral("speakResponses")) {
    value = settings.value(AssistantSpeakResponsesKey, true).toBool() ? 1 : 0;
    return true;
  }

  if (field == QStringLiteral("listenMode")) {
    value = settings
                .value(AssistantListenModeKey,
                       static_cast<int>(Settings::ListenLevel::WakeWord))
                .toInt();
    return true;
  }

  if (field == QStringLiteral("accessSearch")) {
    value = settings
                .value(AssistantAccessSearchKey,
                       static_cast<int>(Settings::AccessLevel::ReadWrite))
                .toInt();
    return true;
  }

  if (field == QStringLiteral("accessTools")) {
    value = settings
                .value(AssistantAccessToolsKey,
                       static_cast<int>(Settings::AccessLevel::ReadWrite))
                .toInt();
    return true;
  }

  if (field == QStringLiteral("accessUserMemory")) {
    value = settings
                .value(AssistantAccessUserMemoryKey,
                       static_cast<int>(Settings::AccessLevel::ReadWrite))
                .toInt();
    return true;
  }

  if (field == QStringLiteral("accessSelfMemory")) {
    value = settings
                .value(AssistantAccessSelfMemoryKey,
                       static_cast<int>(Settings::AccessLevel::ReadWrite))
                .toInt();
    return true;
  }

  if (field == QStringLiteral("reviewGate")) {
    value = settings.value(AssistantReviewGateKey, true).toBool() ? 1 : 0;
    return true;
  }

  if (field == QStringLiteral("activityWatch")) {
    value = settings.value(AssistantActivityWatchKey, true).toBool() ? 1 : 0;
    return true;
  }

  return false;
}

// Clamp a value to the valid range for its field. Returns the clamped
// value. Unknown field returns the input unchanged.
int clampAssistantField(const QString &field, int value) {
  if (field == QStringLiteral("autonomy")) {
    return qBound(static_cast<int>(Settings::AutonomyLevel::Off),
                  value,
                  static_cast<int>(Settings::AutonomyLevel::Autonomous));
  }

  if (field == QStringLiteral("listenMode")) {
    return qBound(static_cast<int>(Settings::ListenLevel::Off),
                  value,
                  static_cast<int>(Settings::ListenLevel::Always));
  }

  if (field == QStringLiteral("accessSearch") ||
      field == QStringLiteral("accessTools") ||
      field == QStringLiteral("accessUserMemory") ||
      field == QStringLiteral("accessSelfMemory")) {
    return qBound(static_cast<int>(Settings::AccessLevel::Off),
                  value,
                  static_cast<int>(Settings::AccessLevel::ReadWrite));
  }

  if (field == QStringLiteral("speakResponses") ||
      field == QStringLiteral("reviewGate") ||
      field == QStringLiteral("activityWatch")) {
    return value != 0 ? 1 : 0;
  }

  return value;
}

} // namespace

QString Settings::getRootDirectory() {
  QSettings settings;
  const QString defaultRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/notes";
  QDir().mkpath(defaultRoot);
  return settings.value("root", defaultRoot).toString();
}

void Settings::setRootDirectory(const QString &newRoot) {
  QSettings settings;
  settings.setValue("root", newRoot);
}

int Settings::getOverseerToolCallDepthLimit() {
  QSettings settings;
  const int value =
      settings.value(OverseerToolCallDepthKey, DefaultToolCallDepth).toInt();
  return qBound(1, value, 64);
}

void Settings::setOverseerToolCallDepthLimit(int limit) {
  QSettings settings;
  settings.setValue(OverseerToolCallDepthKey, qBound(1, limit, 64));
}

int Settings::getOverseerConcurrencyCap() {
  QSettings settings;
  const int value =
      settings.value(OverseerConcurrencyCapKey, DefaultConcurrencyCap).toInt();
  return qBound(1, value, 16);
}

void Settings::setOverseerConcurrencyCap(int cap) {
  QSettings settings;
  settings.setValue(OverseerConcurrencyCapKey, qBound(1, cap, 16));
}

Settings::LlmSettings Settings::getLlmSettings() {
  QSettings settings;

  LlmSettings result;

  const bool alreadySeeded = settings.value(LlmSeededKey, false).toBool();

  if (!alreadySeeded) {
    result.mode =
        qEnvironmentVariable("TALOS_LLM_MODE").trimmed().toLower();
    if (result.mode.isEmpty())
      result.mode = QStringLiteral("local");

    result.endpoint = qEnvironmentVariable("TALOS_LLM_URL").trimmed();
    result.model = qEnvironmentVariable("TALOS_LLM_MODEL").trimmed();
    result.apiKey = qEnvironmentVariable("TALOS_LLM_API_KEY").trimmed();

    result.authType =
        qEnvironmentVariable("TALOS_LLM_AUTH").trimmed().toLower();
    if (result.authType.isEmpty())
      result.authType = QStringLiteral("bearer");

    settings.setValue(LlmModeKey, result.mode);
    settings.setValue(LlmEndpointKey, result.endpoint);
    settings.setValue(LlmModelKey, result.model);
    settings.setValue(LlmApiKeyKey, result.apiKey);
    settings.setValue(LlmAuthTypeKey, result.authType);
    settings.setValue(LlmSeededKey, true);

    return result;
  }

  result.mode = settings.value(LlmModeKey, "local").toString();
  result.endpoint = settings.value(LlmEndpointKey, QString()).toString();
  result.model = settings.value(LlmModelKey, QString()).toString();
  result.apiKey = settings.value(LlmApiKeyKey, QString()).toString();
  result.authType = settings.value(LlmAuthTypeKey, "bearer").toString();

  return result;
}

void Settings::setLlmSettings(const LlmSettings &value) {
  QSettings settings;
  settings.setValue(LlmModeKey, value.mode);
  settings.setValue(LlmEndpointKey, value.endpoint);
  settings.setValue(LlmModelKey, value.model);
  settings.setValue(LlmApiKeyKey, value.apiKey);
  settings.setValue(LlmAuthTypeKey, value.authType);
  settings.setValue(LlmSeededKey, true);
}

int Settings::getOverseerToastDurationMs() {
  QSettings settings;
  const int value =
      settings.value(OverseerToastDurationKey, DefaultToastDurationMs).toInt();
  return qBound(1000, value, 60000);
}

void Settings::setOverseerToastDurationMs(int ms) {
  QSettings settings;
  settings.setValue(OverseerToastDurationKey, qBound(1000, ms, 60000));
}

int Settings::getOverseerFileAgentCap() {
  QSettings settings;
  const int value =
      settings.value(OverseerFileAgentCapKey, DefaultFileAgentCap).toInt();
  return qBound(1, value, 16);
}

void Settings::setOverseerFileAgentCap(int cap) {
  QSettings settings;
  settings.setValue(OverseerFileAgentCapKey, qBound(1, cap, 16));
}

QString Settings::getPayloadLogRoot() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
         QStringLiteral("/logs/payloads");
}

// ---------------------------------------------------------------------
// Assistant settings
// ---------------------------------------------------------------------

Settings::AssistantSettings Settings::getAssistantSettings() {
  QSettings settings;

  AssistantSettings result;

  result.autonomy = static_cast<AutonomyLevel>(
      qBound(static_cast<int>(AutonomyLevel::Off),
             settings
                 .value(AssistantAutonomyKey,
                        static_cast<int>(AutonomyLevel::Suggestive))
                 .toInt(),
             static_cast<int>(AutonomyLevel::Autonomous)));

  result.speakResponses =
      settings.value(AssistantSpeakResponsesKey, true).toBool();

  result.listenMode = static_cast<ListenLevel>(
      qBound(static_cast<int>(ListenLevel::Off),
             settings
                 .value(AssistantListenModeKey,
                        static_cast<int>(ListenLevel::WakeWord))
                 .toInt(),
             static_cast<int>(ListenLevel::Always)));

  auto accessFrom = [&settings](const char *key) {
    return static_cast<AccessLevel>(
        qBound(static_cast<int>(AccessLevel::Off),
               settings.value(key, static_cast<int>(AccessLevel::ReadWrite))
                   .toInt(),
               static_cast<int>(AccessLevel::ReadWrite)));
  };

  result.accessSearch = accessFrom(AssistantAccessSearchKey);
  result.accessTools = accessFrom(AssistantAccessToolsKey);
  result.accessUserMemory = accessFrom(AssistantAccessUserMemoryKey);
  result.accessSelfMemory = accessFrom(AssistantAccessSelfMemoryKey);

  result.reviewGate = settings.value(AssistantReviewGateKey, true).toBool();
  result.activityWatch =
      settings.value(AssistantActivityWatchKey, true).toBool();

  return result;
}

void Settings::setAssistantSettings(const AssistantSettings &value) {
  QSettings settings;

  settings.setValue(AssistantAutonomyKey, static_cast<int>(value.autonomy));
  settings.setValue(AssistantSpeakResponsesKey, value.speakResponses);
  settings.setValue(AssistantListenModeKey, static_cast<int>(value.listenMode));
  settings.setValue(AssistantAccessSearchKey,
                    static_cast<int>(value.accessSearch));
  settings.setValue(AssistantAccessToolsKey,
                    static_cast<int>(value.accessTools));
  settings.setValue(AssistantAccessUserMemoryKey,
                    static_cast<int>(value.accessUserMemory));
  settings.setValue(AssistantAccessSelfMemoryKey,
                    static_cast<int>(value.accessSelfMemory));
  settings.setValue(AssistantReviewGateKey, value.reviewGate);
  settings.setValue(AssistantActivityWatchKey, value.activityWatch);
}

bool Settings::setAssistantField(const QString &field, int value) {
  QSettings settings;

  const int clamped = clampAssistantField(field, value);

  if (field == QStringLiteral("autonomy")) {
    settings.setValue(AssistantAutonomyKey, clamped);
    return true;
  }

  if (field == QStringLiteral("speakResponses")) {
    settings.setValue(AssistantSpeakResponsesKey, clamped != 0);
    return true;
  }

  if (field == QStringLiteral("listenMode")) {
    settings.setValue(AssistantListenModeKey, clamped);
    return true;
  }

  if (field == QStringLiteral("accessSearch")) {
    settings.setValue(AssistantAccessSearchKey, clamped);
    return true;
  }

  if (field == QStringLiteral("accessTools")) {
    settings.setValue(AssistantAccessToolsKey, clamped);
    return true;
  }

  if (field == QStringLiteral("accessUserMemory")) {
    settings.setValue(AssistantAccessUserMemoryKey, clamped);
    return true;
  }

  if (field == QStringLiteral("accessSelfMemory")) {
    settings.setValue(AssistantAccessSelfMemoryKey, clamped);
    return true;
  }

  if (field == QStringLiteral("reviewGate")) {
    settings.setValue(AssistantReviewGateKey, clamped != 0);
    return true;
  }

  if (field == QStringLiteral("activityWatch")) {
    settings.setValue(AssistantActivityWatchKey, clamped != 0);
    return true;
  }

  return false;
}

bool Settings::setAssistantFieldRestricted(const QString &field, int value,
                                           QString *reason) {
  const AssistantWriteDirection direction = assistantWriteDirectionFor(field);

  if (direction == AssistantWriteDirection::None) {
    if (reason) {
      *reason = QStringLiteral(
          "Field '%1' is not writable by the assistant.").arg(field);
    }
    return false;
  }

  QSettings settings;

  int current = 0;

  if (!readAssistantFieldRaw(settings, field, current)) {
    if (reason) {
      *reason = QStringLiteral("Unknown field '%1'.").arg(field);
    }
    return false;
  }

  const int clamped = clampAssistantField(field, value);

  // MoreRestrictive: the new value must rank strictly lower than the
  // current value. Equal is a no-op and is accepted without writing.
  if (clamped == current) {
    return true;
  }

  if (clamped > current) {
    if (reason) {
      *reason = QStringLiteral(
          "Field '%1' may only be made more restrictive by the "
          "assistant. The user can loosen it in Settings.").arg(field);
    }
    return false;
  }

  return setAssistantField(field, clamped);
}

Settings::AssistantWriteDirection Settings::assistantWriteDirectionFor(
    const QString &field) {
  return assistantWriteTable().value(field, AssistantWriteDirection::None);
}

QString Settings::describeAssistantField(const QString &field) {
  const AssistantSettings current = getAssistantSettings();

  if (field == QStringLiteral("autonomy")) {
    return autonomyToString(current.autonomy);
  }
  if (field == QStringLiteral("speakResponses")) {
    return current.speakResponses ? QStringLiteral("on")
                                  : QStringLiteral("off");
  }
  if (field == QStringLiteral("listenMode")) {
    return listenLevelToString(current.listenMode);
  }
  if (field == QStringLiteral("accessSearch")) {
    return accessLevelToString(current.accessSearch);
  }
  if (field == QStringLiteral("accessTools")) {
    return accessLevelToString(current.accessTools);
  }
  if (field == QStringLiteral("accessUserMemory")) {
    return accessLevelToString(current.accessUserMemory);
  }
  if (field == QStringLiteral("accessSelfMemory")) {
    return accessLevelToString(current.accessSelfMemory);
  }
  if (field == QStringLiteral("reviewGate")) {
    return current.reviewGate ? QStringLiteral("on") : QStringLiteral("off");
  }
  if (field == QStringLiteral("activityWatch")) {
    return current.activityWatch ? QStringLiteral("on")
                                 : QStringLiteral("off");
  }

  return QStringLiteral("unknown");
}

QString Settings::autonomyToString(AutonomyLevel level) {
  switch (level) {
  case AutonomyLevel::Off:
    return QStringLiteral("off");
  case AutonomyLevel::Explicit:
    return QStringLiteral("explicit");
  case AutonomyLevel::Suggestive:
    return QStringLiteral("suggestive");
  case AutonomyLevel::Autonomous:
    return QStringLiteral("autonomous");
  }
  return QStringLiteral("suggestive");
}

Settings::AutonomyLevel Settings::autonomyFromString(
    const QString &value, AutonomyLevel fallback) {
  const QString normalized = value.trimmed().toLower();

  if (normalized == QStringLiteral("off"))
    return AutonomyLevel::Off;
  if (normalized == QStringLiteral("explicit"))
    return AutonomyLevel::Explicit;
  if (normalized == QStringLiteral("suggestive"))
    return AutonomyLevel::Suggestive;
  if (normalized == QStringLiteral("autonomous"))
    return AutonomyLevel::Autonomous;

  return fallback;
}

QString Settings::listenLevelToString(ListenLevel level) {
  switch (level) {
  case ListenLevel::Off:
    return QStringLiteral("off");
  case ListenLevel::WakeWord:
    return QStringLiteral("wakeword");
  case ListenLevel::Always:
    return QStringLiteral("always");
  }
  return QStringLiteral("wakeword");
}

Settings::ListenLevel Settings::listenLevelFromString(
    const QString &value, ListenLevel fallback) {
  const QString normalized = value.trimmed().toLower();

  if (normalized == QStringLiteral("off"))
    return ListenLevel::Off;
  if (normalized == QStringLiteral("wakeword") ||
      normalized == QStringLiteral("wake-word") ||
      normalized == QStringLiteral("wake_word"))
    return ListenLevel::WakeWord;
  if (normalized == QStringLiteral("always"))
    return ListenLevel::Always;

  return fallback;
}

QString Settings::accessLevelToString(AccessLevel level) {
  switch (level) {
  case AccessLevel::Off:
    return QStringLiteral("off");
  case AccessLevel::Read:
    return QStringLiteral("read");
  case AccessLevel::ReadWrite:
    return QStringLiteral("readwrite");
  }
  return QStringLiteral("off");
}

Settings::AccessLevel Settings::accessLevelFromString(
    const QString &value, AccessLevel fallback) {
  const QString normalized = value.trimmed().toLower();

  if (normalized == QStringLiteral("off"))
    return AccessLevel::Off;
  if (normalized == QStringLiteral("read"))
    return AccessLevel::Read;
  if (normalized == QStringLiteral("readwrite") ||
      normalized == QStringLiteral("read-write") ||
      normalized == QStringLiteral("read_write"))
    return AccessLevel::ReadWrite;

  return fallback;
}