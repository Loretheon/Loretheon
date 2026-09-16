#pragma once

#include "OverseerTool.h"

// A tool that proposes a durable fact for the user to accept into the
// global Memory file. It does not write anything; it just returns a
// short acknowledgement and lets the widget render a review card.
//
// The actual write to memory.md happens when the user clicks ✓ on the
// proposal card in the transcript.

class ProposeMemoryFactTool : public OverseerTool {
public:
  QString name() const override {
    return QStringLiteral("propose_memory_fact");
  }

  QString description() const override {
    return QStringLiteral(
        "Propose a durable fact to be added to the user's global Memory "
        "file. The user will review and approve or reject the proposal. "
        "Use this only for facts that should persist across sessions; do "
        "not use it for session-specific state.");
  }

  QJsonObject parametersSchema() const override;

  Result execute(const QJsonObject &arguments,
                 const Context &context) const override;
};