#ifndef EPISTEME_VOICECOMMANDREGISTRY_H
#define EPISTEME_VOICECOMMANDREGISTRY_H

#include "VoiceCommand.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

// Holds the registered voice commands. Commands are added once at
// startup and live for the lifetime of the registry. The registry
// emits changed() when a command's running state changes so the panel
// can update without polling.
class VoiceCommandRegistry : public QObject {
  Q_OBJECT

public:
  explicit VoiceCommandRegistry(QObject *parent = nullptr);
  ~VoiceCommandRegistry() override;

  // Take ownership of a command. The registry does not accept null.
  void add(std::unique_ptr<VoiceCommand> command);

  // Look up by id. Returns null if no such command.
  VoiceCommand *byId(const QString &id) const;

  // All registered commands, in registration order.
  QVector<VoiceCommand *> all() const;

  // Notify listeners that a command's state changed. Callers of
  // start()/stop() on a command should invoke this afterwards.
  void notifyChanged();

  signals:
    void changed();

private:
  std::vector<std::unique_ptr<VoiceCommand>> m_commands;
};

#endif // EPISTEME_VOICECOMMANDREGISTRY_H