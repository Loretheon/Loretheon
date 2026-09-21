#include "../../include/voice/VoiceCommandRegistry.h"

VoiceCommandRegistry::VoiceCommandRegistry(QObject *parent) : QObject(parent) {}

VoiceCommandRegistry::~VoiceCommandRegistry() = default;

void VoiceCommandRegistry::add(std::unique_ptr<VoiceCommand> command) {
  if (!command) {
    return;
  }
  m_commands.push_back(std::move(command));
  emit changed();
}

VoiceCommand *VoiceCommandRegistry::byId(const QString &id) const {
  for (const auto &command : m_commands) {
    if (command && command->id() == id) {
      return command.get();
    }
  }
  return nullptr;
}

QVector<VoiceCommand *> VoiceCommandRegistry::all() const {
  QVector<VoiceCommand *> result;
  result.reserve(static_cast<int>(m_commands.size()));
  for (const auto &command : m_commands) {
    if (command) {
      result.append(command.get());
    }
  }
  return result;
}

void VoiceCommandRegistry::notifyChanged() { emit changed(); }