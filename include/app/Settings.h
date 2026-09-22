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

  // Access tiers, ordered from least to most permissive.
  enum class AccessLevel {
    Off = 0,
    Read = 1,
    ReadWrite = 2,
  };

  // Autonomy policy, ordered from least to most permissive.
  enum class AutonomyLevel {
    Off = 0,
    Explicit = 1,
    Suggestive = 2,
    Autonomous = 3,
  };

  // Listening policy, ordered from least to most permissive.
  enum class ListenLevel {
    Off = 0,
    WakeWord = 1,
    Always = 2,
  };

  struct AssistantSettings {
    AutonomyLevel autonomy = AutonomyLevel::Suggestive;
    bool speakResponses = true;
    ListenLevel listenMode = ListenLevel::WakeWord;
    AccessLevel accessSearch = AccessLevel::ReadWrite;
    AccessLevel accessTools = AccessLevel::ReadWrite;
    AccessLevel accessUserMemory = AccessLevel::ReadWrite;
    AccessLevel accessSelfMemory = AccessLevel::ReadWrite;
    bool reviewGate = true;
    bool activityWatch = true;
  };

  // The direction a field may be moved in by the assistant. Every
  // field the assistant can write is MoreRestrictive, meaning the new
  // value must rank lower than the old one on its ordering. Fields
  // that are not writable by the assistant return None.
  enum class AssistantWriteDirection {
    None,
    MoreRestrictive,
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

  // -----------------------------------------------------------------
  // Assistant settings
  // -----------------------------------------------------------------

  static AssistantSettings getAssistantSettings();
  static void setAssistantSettings(const AssistantSettings &settings);

  // Persist a single field by name. Field names match the struct
  // members exactly: "autonomy", "speakResponses", "listenMode",
  // "accessSearch", "accessTools", "accessUserMemory",
  // "accessSelfMemory", "reviewGate", "activityWatch".
  //
  // Returns true when the value was valid and written. The assistant
  // path calls setAssistantFieldRestricted, which additionally checks
  // that the change moves the field in the allowed direction.
  static bool setAssistantField(const QString &field, int value);

  static bool setAssistantFieldRestricted(const QString &field, int value,
                                          QString *reason = nullptr);

  // The write direction for a field, or None if the assistant may not
  // write it at all.
  static AssistantWriteDirection assistantWriteDirectionFor(
      const QString &field);

  // Human-readable current value of a field, for prompts and for the
  // assistant to describe its own configuration to the user.
  static QString describeAssistantField(const QString &field);

  // -----------------------------------------------------------------
  // String conversions. Used by the settings dialog and by prompts.
  // -----------------------------------------------------------------

  static QString autonomyToString(AutonomyLevel level);
  static AutonomyLevel autonomyFromString(const QString &value,
                                          AutonomyLevel fallback);

  static QString listenLevelToString(ListenLevel level);
  static ListenLevel listenLevelFromString(const QString &value,
                                           ListenLevel fallback);

  static QString accessLevelToString(AccessLevel level);
  static AccessLevel accessLevelFromString(const QString &value,
                                           AccessLevel fallback);
};

#endif // EPISTEME_SETTINGS_H