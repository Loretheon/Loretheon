#ifndef EPISTEME_VOICECOMMAND_H
#define EPISTEME_VOICECOMMAND_H

#include <QString>

class InferenceService;
class TextEdit;

// The context a command operates on. Passed by the caller so that
// commands do not need to reach into MainWindow or any UI directly.
struct VoiceContext {
  // The editor currently focused, or null if none. Commands that write
  // into a document should no-op cleanly when this is null.
  TextEdit *editor = nullptr;

  // The inference service. Never null in a running application.
  InferenceService *inference = nullptr;
};

// A single voice-driven action. Implementations are registered with
// VoiceCommandRegistry; the panel and any future trigger surface them
// by id or by enumeration.
//
// Commands must be stateless between invocations, or hold all state
// internally and expose start()/stop() cleanly. They are owned by the
// registry and live for the lifetime of the application.
class VoiceCommand {
public:
  virtual ~VoiceCommand() = default;

  // Stable identifier. Used for dispatch and persistence.
  virtual QString id() const = 0;

  // Human-readable label shown in the panel.
  virtual QString title() const = 0;

  // One-line description of what the command does.
  virtual QString description() const = 0;

  // True if the command can run right now given the context. The panel
  // greys out commands for which this returns false.
  virtual bool canRun(const VoiceContext &context) const = 0;

  // True while the command is doing something — listening, speaking,
  // transcribing. The panel shows live state for running commands.
  virtual bool isRunning() const = 0;

  // Start the command. Commands that complete on their own emit their
  // own completion; commands that run until stopped should return
  // immediately and stay running.
  virtual void start(const VoiceContext &context) = 0;

  // Stop the command if it is running. Safe to call when not running.
  virtual void stop() = 0;
};

#endif // EPISTEME_VOICECOMMAND_H