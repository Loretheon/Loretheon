#ifndef EPISTEME_SETTINGS_H
#define EPISTEME_SETTINGS_H

#include <QString>

class Settings {
public:
  struct LlmSettings {
    QString mode;      // "local" or "remote"
    QString endpoint;
    QString model;
    QString apiKey;
    QString authType;  // "none" or "bearer"
  };

  static QString getRootDirectory();
  static void setRootDirectory(const QString &newRoot);

  static int getOverseerToolCallDepthLimit();
  static void setOverseerToolCallDepthLimit(int limit);

  // LLM configuration. On first read, if the settings keys are absent,
  // they are seeded from the legacy TALOS_LLM_* environment variables and
  // written back. Later reads never consult the environment again.
  static LlmSettings getLlmSettings();
  static void setLlmSettings(const LlmSettings &settings);
};

#endif // EPISTEME_SETTINGS_H