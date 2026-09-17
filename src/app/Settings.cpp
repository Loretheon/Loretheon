#include "../../include/app/Settings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr auto OverseerToolCallDepthKey = "overseer/toolCallDepthLimit";
constexpr int DefaultToolCallDepth = 16;

constexpr auto LlmModeKey = "llm/mode";
constexpr auto LlmEndpointKey = "llm/endpoint";
constexpr auto LlmModelKey = "llm/model";
constexpr auto LlmApiKeyKey = "llm/apiKey";
constexpr auto LlmAuthTypeKey = "llm/authType";
constexpr auto LlmSeededKey = "llm/seededFromEnv";

constexpr auto OverseerToastDurationKey = "overseer/toastDurationMs";
constexpr int DefaultToastDurationMs = 8000;

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