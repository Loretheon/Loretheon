#ifndef EPISTEME_SETTINGS_H
#define EPISTEME_SETTINGS_H

#include <QString>

class Settings {
public:
  struct LlmSettings {
    QString mode;
    QString endpoint;
    QString model;
    QString apiKey;
    QString authType;
  };

  static QString getRootDirectory();
  static void setRootDirectory(const QString &newRoot);

  static int getOverseerToolCallDepthLimit();
  static void setOverseerToolCallDepthLimit(int limit);

  static int getOverseerConcurrencyCap();
  static void setOverseerConcurrencyCap(int cap);

  static LlmSettings getLlmSettings();
  static void setLlmSettings(const LlmSettings &settings);

  static int getOverseerToastDurationMs();
  static void setOverseerToastDurationMs(int ms);

  static int getOverseerFileAgentCap();
  static void setOverseerFileAgentCap(int cap);

  static QString getPayloadLogRoot();
};

#endif // EPISTEME_SETTINGS_H