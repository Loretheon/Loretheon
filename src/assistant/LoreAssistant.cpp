#include "../../include/assistant/LoreAssistant.h"

#include "../../include/app/DocumentManager.h"
#include "../../include/assistant/AssistantActivity.h"
#include "../../include/assistant/AssistantMemory.h"
#include "../../include/assistant/AssistantProfile.h"
#include "../../include/assistant/AssistantToolRegistry.h"
#include "../../include/assistant/MemoryIndex.h"
#include "../../include/assistant/SpeechAnimator.h"
#include "../../include/assistant/tools/AssistantTools.h"
#include "../../include/avatar/AvatarWidget.h"
#include "../../include/search/SearchService.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QDir>

LoreAssistant::LoreAssistant(const Config &config, QObject *parent)
    : QObject(parent), m_config(config) {
  m_profile = new AssistantProfile();
  m_memory = new AssistantMemory();
  m_activity = new AssistantActivity(this);
  m_tools = new assistant::AssistantToolRegistry();

  m_animator = new SpeechAnimator(m_config.inference, m_config.avatar, this);

  if (m_config.inference) {
    m_memoryIndex = new MemoryIndex(m_config.inference, this);
  }
}

LoreAssistant::~LoreAssistant() {
  stop();

  delete m_tools;
  delete m_memory;
  delete m_profile;
}

bool LoreAssistant::start() {
  if (m_started) {
    return true;
  }

  if (m_config.root.isEmpty()) {
    qWarning() << "[LoreAssistant] No assistant root configured.";
    return false;
  }

  QDir().mkpath(m_config.root);

  m_profile->setRoot(m_config.root);

  if (!m_profile->load()) {
    qWarning() << "[LoreAssistant] Profile could not be loaded from"
               << m_config.root;
    return false;
  }

  m_memory->setRoot(m_config.root);

  if (!m_memory->ensureRoot()) {
    qWarning() << "[LoreAssistant] Memory tree could not be created under"
               << m_config.root;
    return false;
  }

  if (m_memoryIndex) {
    m_memoryIndex->setMemoryRoot(
        QDir(m_config.root).filePath(QStringLiteral("memories")));

    if (!m_memoryIndex->load()) {
      // No memory index on disk yet. Building one embeds every memory
      // file and is slow; it is not done here. Recall returns empty
      // until the index is rebuilt, which is acceptable for Step A.
      qDebug() << "[LoreAssistant] No memory index on disk yet; "
                  "recall will be empty until it is rebuilt.";
    }

    m_memory->setMemoryIndex(m_memoryIndex);
  }

  if (m_config.inference) {
    m_memory->setInference(m_config.inference);
  }

  assistant::AssistantTools::installAll(*m_tools);

  m_activity->startBatchTimer();

  m_started = true;

  qDebug() << "[LoreAssistant] Started. Root:" << m_config.root
           << "Tools:" << !m_tools->isEmpty();

  return true;
}

void LoreAssistant::stop() {
  if (!m_started) {
    return;
  }

  m_activity->stopBatchTimer();

  if (m_animator) {
    m_animator->reset();
  }

  m_started = false;
}

void LoreAssistant::say(const QString &text) {
  if (text.isEmpty() || !m_config.inference) {
    return;
  }

  m_config.inference->speak(text);

  emit assistantSaid(text);
}