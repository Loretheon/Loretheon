#pragma once

#include "AssistantToolRegistry.h"

#include <QObject>
#include <QString>

namespace assistant {
class AssistantToolRegistry;
}
class AssistantActivity;
class AssistantMemory;
class AssistantProfile;
class AssistantToolRegistry;
class AvatarWidget;
class DocumentManager;
class InferenceService;
class MemoryIndex;
class SearchService;
class SpeechAnimator;

// The conductor. Owns the assistant's long-lived state, wires it to
// the rest of the application, and — in a later step — runs the LLM
// loop that turns a user message or an activity batch into speech,
// tool calls, or silence.
//
// Step A (this file) is the container only. It constructs the profile,
// memory, context, activity stream, tool registry, and speech
// animator, wires them together, and provides accessors. It does not
// yet talk to the LLM.
//
// LoreAssistant owns none of the application objects it is given. It
// holds raw pointers to them for the duration of its life, which is
// the same duration as MainWindow's.
class LoreAssistant : public QObject {
  Q_OBJECT

public:
  // Everything LoreAssistant needs from the application, passed in
  // explicitly so it does not reach into MainWindow.
  struct Config {
    InferenceService *inference = nullptr;
    AvatarWidget *avatar = nullptr;
    DocumentManager *documents = nullptr;
    SearchService *search = nullptr;

    // Absolute path to the assistant's root, e.g.
    // ~/.local/share/Questfarer/Lore/assistant. Created on first run.
    QString root;
  };

  explicit LoreAssistant(const Config &config,
                         QObject *parent = nullptr);
  ~LoreAssistant() override;

  // Load the profile, ensure the memory tree, start the activity
  // batch timer, and hand the speech animator to the avatar. Safe to
  // call once. Returns false if the assistant root cannot be created.
  bool start();

  // Stop timers and flush. Called from MainWindow's destructor path.
  void stop();

  // Speak a line. The mouth animation follows automatically through
  // the SpeechAnimator, which is subscribed to the inference service.
  void say(const QString &text);

  AssistantProfile *profile() const { return m_profile; }
  AssistantMemory *memory() const { return m_memory; }
  AssistantActivity *activity() const { return m_activity; }
  SpeechAnimator *animator() const { return m_animator; }
  assistant::AssistantToolRegistry *tools() const { return m_tools; }

signals:
  // Emitted when the assistant has something the user should see in a
  // transcript. Step B fills this in; for now it never fires.
  void assistantSaid(const QString &text);

private:
  Config m_config;

  AssistantProfile *m_profile = nullptr;
  AssistantMemory *m_memory = nullptr;
  MemoryIndex *m_memoryIndex = nullptr;
  AssistantActivity *m_activity = nullptr;
  assistant::AssistantToolRegistry *m_tools = nullptr;
  SpeechAnimator *m_animator = nullptr;

  bool m_started = false;
};